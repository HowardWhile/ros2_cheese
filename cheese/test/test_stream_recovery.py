# Copyright 2026 Howard
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""Exercise recovery through launch, ROS messages, status, and capture services."""
import json
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time
import unittest

from cheese_interfaces.srv import StringTrigger
import cv2
import numpy as np
import rclpy
from rclpy.parameter import Parameter
from rclpy.parameter_client import AsyncParameterClient
from sensor_msgs.msg import CompressedImage, Image
from std_msgs.msg import String
from std_srvs.srv import Trigger


class StreamRecovery(unittest.TestCase):
    def setUp(self):
        self.context = rclpy.context.Context()
        self.context.init(domain_id=175)
        self.addCleanup(self.context.try_shutdown)
        self.node = rclpy.create_node('recovery_test', context=self.context)
        self.addCleanup(self.node.destroy_node)
        self.executor = rclpy.executors.SingleThreadedExecutor(context=self.context)
        self.executor.add_node(self.node)
        self.addCleanup(self.executor.shutdown)
        self.directory = tempfile.TemporaryDirectory(prefix='cheese-recovery-')
        self.addCleanup(self.directory.cleanup)
        self.capture_dir = Path(self.directory.name) / 'captures'
        self.log = open(Path(self.directory.name) / 'launch.log', 'w+')
        self.addCleanup(self.log.close)
        self.sources = []
        self.addCleanup(self.cleanup_sources)
        if (self._testMethodName ==
                'test_initial_raw_preference_and_bidirectional_disconnect_recovery'):
            self.source('raw', 180)
            self.source('compressed', 70)
            deadline = time.monotonic() + 5
            while len(self.node.get_publishers_info_by_topic('/camera/image')) < 2:
                self.executor.spin_once(timeout_sec=0.05)
                self.assertLess(time.monotonic(), deadline)

        env = dict(os.environ, ROS_DOMAIN_ID='175')
        if self._testMethodName == 'test_subscription_creation_failure_retries':
            env['LD_PRELOAD'] = os.environ['CHEESE_TEST_FAILURE_LIBRARY']
        self.process = subprocess.Popen(
            ['ros2', 'launch', 'cheese', 'cheese.launch.py',
             'image_topic:=/camera/image', f'capture_dir:={self.capture_dir}'],
            env=env, stdout=self.log, stderr=subprocess.STDOUT,
            start_new_session=True, cwd=self.directory.name)
        self.addCleanup(self.stop_launch)
        self.status = None
        self.history = []
        self.subscription = self.node.create_subscription(
            String, '/cheese/status', self.receive_status, 10)
        self.trigger = self.node.create_client(Trigger, '/cheese/trigger')
        self.string_trigger = self.node.create_client(
            StringTrigger, '/cheese/string_trigger')
        self.wait(lambda: self.status is not None)

    def stop_launch(self):
        if self.process.poll() is None:
            os.killpg(self.process.pid, signal.SIGINT)
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                os.killpg(self.process.pid, signal.SIGKILL)
                self.process.wait()
        self.log.seek(0)
        print(self.log.read())

    def cleanup_sources(self):
        for source in list(self.sources):
            self.remove(source)

    def receive_status(self, message):
        self.status = json.loads(message.data)
        self.history.append((time.monotonic(), self.status))

    def pump(self, seconds):
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            for source in self.sources:
                if source['mode'] != 'silent':
                    source['publisher'].publish(source['message'])
            self.executor.spin_once(timeout_sec=0.025)
            self.assertIsNone(self.process.poll(), 'launch exited')
            self.log.flush()
            self.log.seek(0)
            self.assertNotIn('process has died', self.log.read())

    def wait(self, predicate, timeout=8):
        deadline = time.monotonic() + timeout
        while not predicate() and time.monotonic() < deadline:
            self.pump(0.05)
        self.assertTrue(predicate(), f'timed out; status={self.status}')

    def source(self, kind, value):
        frame = np.full((24, 32, 3), value, dtype=np.uint8)
        if kind == 'raw':
            message = Image(height=24, width=32, encoding='bgr8',
                            step=96, data=frame.tobytes())
            msg_type = Image
        else:
            ok, data = cv2.imencode('.jpg', frame)
            self.assertTrue(ok)
            message = CompressedImage(format='jpeg', data=data.tobytes())
            msg_type = CompressedImage
        context = rclpy.context.Context()
        context.init(domain_id=175)
        self.addCleanup(context.try_shutdown)
        node = rclpy.create_node('source_' + kind + '_' + str(len(self.sources)), context=context)
        self.addCleanup(node.destroy_node)
        source = {'publisher': node.create_publisher(
            msg_type, '/camera/image', 10), 'message': message, 'mode': 'valid',
            'node': node, 'context': context}
        self.sources.append(source)
        return source

    def remove(self, source):
        self.sources.remove(source)
        source['node'].destroy_node()
        source['context'].try_shutdown()

    def healthy(self, kind):
        return (self.status['subscription_type'] == kind
                and self.status['stream_ok']
                and self.status['window']['frames'] > 0)

    def capture(self, value, tagged=False):
        client = self.string_trigger if tagged else self.trigger
        self.wait(lambda: client.service_is_ready())
        request = StringTrigger.Request(message='source') if tagged else Trigger.Request()
        future = client.call_async(request)
        self.wait(future.done)
        response = future.result()
        self.assertTrue(response.success, response.message)
        image = cv2.imread(response.message)
        self.assertIsNotNone(image)
        self.assertLess(abs(float(image.mean()) - value), 2)
        if tagged:
            self.assertIn('-source.jpg', response.message)

    def test_compressed_stays_selected_during_raw_churn(self):
        self.source('compressed', 70)
        self.wait(lambda: self.healthy('compressed'))
        raw = self.source('raw', 180)
        self.pump(4)
        self.assertTrue(self.healthy('compressed'))
        self.remove(raw)
        self.pump(4)
        self.assertTrue(self.healthy('compressed'))
        self.capture(70)
        self.capture(70, tagged=True)

    def rejected_capture(self, tagged=False):
        before = set(self.capture_dir.glob('*.jpg'))
        client = self.string_trigger if tagged else self.trigger
        self.wait(lambda: client.service_is_ready())
        request = StringTrigger.Request(message='source') if tagged else Trigger.Request()
        future = client.call_async(request)
        self.wait(future.done)
        response = future.result()
        self.assertFalse(response.success, response.message)
        self.assertTrue(response.message)
        self.assertEqual(before, set(self.capture_dir.glob('*.jpg')))
        return response.message

    def test_stale_capture_is_rejected_and_recovers(self):
        source = self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        self.capture(180)
        source['mode'] = 'silent'
        self.pump(3.3)
        self.assertIn('timeout', self.rejected_capture().lower())
        self.assertIn('timeout', self.rejected_capture(tagged=True).lower())
        source['mode'] = 'valid'
        self.wait(lambda: self.healthy('raw'))
        self.capture(180, tagged=True)

    def test_silent_publisher_switches_to_live_alternative(self):
        source = self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        self.source('compressed', 70)
        self.pump(2)
        self.assertTrue(self.healthy('raw'))
        source['mode'] = 'silent'
        self.wait(lambda: self.healthy('compressed'))
        self.capture(70)
        self.capture(70, tagged=True)

    def test_empty_compressed_frames_do_not_crash_and_recover(self):
        source = self.source('compressed', 70)
        self.wait(lambda: self.healthy('compressed'))
        valid = source['message']
        source['message'] = CompressedImage(format='jpeg', data=b'')
        self.pump(3.5)
        self.assertGreater(self.status['total_failures'], 0)
        self.rejected_capture()
        self.rejected_capture(tagged=True)
        source['message'] = valid
        self.wait(lambda: self.healthy('compressed'))
        self.capture(70)

    def test_subscription_creation_failure_retries(self):
        start = time.monotonic()
        self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        self.assertGreaterEqual(time.monotonic() - start, 2)
        self.assertTrue(any(not status['subscribed'] for _, status in self.history))
        self.log.seek(0)
        self.assertIn('injected subscription creation failure', self.log.read())
        self.capture(180)

    def test_initial_raw_preference_and_bidirectional_disconnect_recovery(self):
        raw, compressed = self.sources
        self.wait(lambda: self.healthy('raw'))
        extra_raw = self.source('raw', 180)
        self.wait(lambda: extra_raw['publisher'].get_subscription_count() > 0)
        self.remove(raw)
        self.pump(2)
        self.assertTrue(self.healthy('raw'))
        self.remove(extra_raw)
        self.wait(lambda: self.healthy('compressed'))
        self.capture(70)
        self.remove(compressed)
        self.wait(lambda: not self.status['subscribed'])
        self.rejected_capture()
        self.rejected_capture(tagged=True)
        self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        self.capture(180, tagged=True)

    def test_switch_waits_for_new_image_and_ignores_retired_image(self):
        raw = self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        compressed = self.source('compressed', 70)
        compressed['mode'] = 'silent'
        self.remove(raw)
        self.wait(lambda: self.status['subscription_type'] == 'compressed')
        self.assertFalse(self.status['stream_ok'])
        self.assertIn('switch', self.rejected_capture().lower())
        self.assertIn('switch', self.rejected_capture(tagged=True).lower())
        compressed['mode'] = 'valid'
        self.wait(lambda: self.healthy('compressed'))
        self.capture(70)

    def test_no_initial_image_and_raw_decode_failure_recover(self):
        self.rejected_capture()
        self.rejected_capture(tagged=True)
        source = self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        valid = source['message']
        source['message'] = Image(height=24, width=32, encoding='bgr8', step=96, data=b'')
        self.pump(3.4)
        self.assertGreater(self.status['total_failures'], 0)
        self.assertIn('timeout', self.rejected_capture().lower())
        source['message'] = valid
        self.wait(lambda: self.healthy('raw'))
        self.capture(180)

    def test_invalid_compressed_switches_and_silent_sources_do_not_flap(self):
        compressed = self.source('compressed', 70)
        self.wait(lambda: self.healthy('compressed'))
        raw = self.source('raw', 180)
        compressed['message'] = CompressedImage(format='jpeg', data=b'bad jpeg')
        self.wait(lambda: self.healthy('raw'))
        self.assertGreater(self.status['total_failures'], 0)
        self.capture(180)
        compressed['mode'] = 'silent'
        raw['mode'] = 'silent'
        index = len(self.history)
        self.pump(11)
        changes = []
        previous = self.history[index - 1][1]['subscription_type']
        for stamp, status in self.history[index:]:
            if status['subscription_type'] != previous:
                changes.append(stamp)
                previous = status['subscription_type']
        self.assertGreaterEqual(len(changes), 2)
        for earlier, later in zip(changes, changes[1:]):
            # Status sampling jitter is allowed; switches must span >=3 ticks.
            self.assertGreaterEqual(later - earlier, 2.8)
        self.assertFalse(self.status['stream_ok'])

    def test_compressed_disconnect_recovers_raw(self):
        compressed = self.source('compressed', 70)
        self.wait(lambda: self.healthy('compressed'))
        self.source('raw', 180)
        self.pump(2)
        self.assertTrue(self.healthy('compressed'))
        self.remove(compressed)
        self.wait(lambda: self.healthy('raw'))
        self.capture(180)
        self.capture(180, tagged=True)

    def test_paused_ros_clock_does_not_block_timeout_recovery(self):
        raw = self.source('raw', 180)
        self.wait(lambda: self.healthy('raw'))
        parameters = AsyncParameterClient(self.node, '/cheese')
        self.wait(parameters.services_are_ready)
        future = parameters.set_parameters([Parameter('use_sim_time', value=True)])
        self.wait(future.done)
        self.assertTrue(future.result().results[0].successful)
        raw['mode'] = 'silent'
        self.pump(3.3)
        self.assertIn('timeout', self.rejected_capture().lower())
        self.source('compressed', 70)
        self.wait(lambda: self.healthy('compressed'))
        self.capture(70)


if __name__ == '__main__':
    unittest.main()
