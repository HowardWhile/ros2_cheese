// Copyright 2026 Howard
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// Test-only middleware fault injection; the real node still runs under launch.
#include <cstring>
#include <dlfcn.h>

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

extern "C" rmw_subscription_t *rmw_create_subscription(
    const rmw_node_t *node,
    const rosidl_message_type_support_t *type_support,
    const char *topic_name,
    const rmw_qos_profile_t *qos,
    const rmw_subscription_options_t *options)
{
    static int failures = 0;
    if (std::strcmp(node->name, "cheese") == 0 && std::strcmp(topic_name, "/camera/image") == 0 && failures < 2)
    {
        ++failures;
        RMW_SET_ERROR_MSG("injected subscription creation failure");
        return nullptr;
    }
    using CreateSubscription = decltype(&rmw_create_subscription);
    static const auto real_create = reinterpret_cast<CreateSubscription>(dlsym(RTLD_NEXT, "rmw_create_subscription"));
    return real_create(node, type_support, topic_name, qos, options);
}
