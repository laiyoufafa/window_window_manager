/*
 * Copyright (c) 2022-2022 Huawei Device Co., Ltd.
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

#ifndef WINDOW_EXTENSION_CLIENT_PROXY_H
#define WINDOW_EXTENSION_CLIENT_PROXY_H

#include "window_extension_client_interface.h"
#include "iremote_proxy.h"

namespace OHOS {
namespace Rosen {
class WindowExtensionClientProxy : public IRemoteProxy<IWindowExtensionClient> {
    explicit WindowExtensionClientProxy(const sptr<IRemoteObject>& impl)
        : IRemoteProxy<IWindowExtensionClient>(impl) {};
    ~WindowExtensionClientProxy() {};

    bool ConnectToClient(sptr<IWindowExtensionServer> token) override;
    void Resize(Rect rect) = 0;
    void Hide() = 0;
    void Show() = 0;
    void RequestFocus() = 0;
};
} // namespace Rosen
} // namespace OHOS
#endif // WINDOW_EXTENSION_CLIENT_PROXY_H