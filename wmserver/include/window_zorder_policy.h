/*
 * Copyright (c) 2021 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OHOS_ROSEN_WINDOW_ZORDER_POLICY_H
#define OHOS_ROSEN_WINDOW_ZORDER_POLICY_H

#include <map>
#include <refbase.h>

#include "wm_common.h"

namespace OHOS {
namespace Rosen {
class WindowZorderPolicy : public RefBase {
public:
    WindowZorderPolicy() = default;
    ~WindowZorderPolicy() = default;

    int32_t GetWindowPriority(WindowType type) const;

private:
    const std::map<WindowType, int32_t> windowPriorityMap_ {
        // sub-windows types
        { WindowType::WINDOW_TYPE_MEDIA,                 -1 },
        { WindowType::WINDOW_TYPE_APP_SUB_WINDOW,         1 },

        // main window
        { WindowType::WINDOW_TYPE_APP_MAIN_WINDOW,        0 },

        // system-specific window
        { WindowType::WINDOW_TYPE_WALLPAPER,            101 },
        { WindowType::WINDOW_TYPE_APP_LAUNCHING,        102 },
        { WindowType::WINDOW_TYPE_DOCK_SLICE,           0 },
        { WindowType::WINDOW_TYPE_INCOMING_CALL,        104 },
        { WindowType::WINDOW_TYPE_SEARCHING_BAR,        105 },
        { WindowType::WINDOW_TYPE_SYSTEM_ALARM_WINDOW,  106 },
        { WindowType::WINDOW_TYPE_INPUT_METHOD_FLOAT,   107 },
        { WindowType::WINDOW_TYPE_FLOAT,                108 },
        { WindowType::WINDOW_TYPE_TOAST,                109 },
        { WindowType::WINDOW_TYPE_STATUS_BAR,           110 },
        { WindowType::WINDOW_TYPE_PANEL,                111 },
        { WindowType::WINDOW_TYPE_KEYGUARD,             112 },
        { WindowType::WINDOW_TYPE_VOLUME_OVERLAY,       113 },
        { WindowType::WINDOW_TYPE_NAVIGATION_BAR,       114 },
        { WindowType::WINDOW_TYPE_DRAGGING_EFFECT,      115 },
        { WindowType::WINDOW_TYPE_POINTER,              116 },
    };
};
}
}
#endif // OHOS_ROSEN_WINDOW_STATE_H
