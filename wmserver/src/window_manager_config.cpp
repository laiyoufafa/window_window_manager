/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "window_manager_config.h"
#include "window_helper.h"
#include "window_manager_hilog.h"

namespace OHOS {
namespace Rosen {
namespace {
    constexpr HiviewDFX::HiLogLabel LABEL = {LOG_CORE, HILOG_DOMAIN_WINDOW, "WindowManagerConfig"};
}

std::map<std::string, bool> WindowManagerConfig::enableConfig_;
std::map<std::string, std::vector<int>> WindowManagerConfig::numbersConfig_;

bool WindowManagerConfig::LoadConfigXml(const std::string& configFilePath)
{
    xmlDocPtr docPtr = xmlReadFile(configFilePath.c_str(), nullptr, XML_PARSE_NOBLANKS);
    WLOGFI("[WmConfig] filePath: %{public}s", configFilePath.c_str());
    if (docPtr == nullptr) {
        WLOGFE("[WmConfig] load xml error!");
        return false;
    }

    xmlNodePtr rootPtr = xmlDocGetRootElement(docPtr);
    if (rootPtr == nullptr || rootPtr->name == nullptr ||
        xmlStrcmp(rootPtr->name, reinterpret_cast<const xmlChar*>("Configs"))) {
        WLOGFE("[WmConfig] get root element failed!");
        xmlFreeDoc(docPtr);
        return false;
    }

    for (xmlNodePtr curNodePtr = rootPtr->xmlChildrenNode; curNodePtr != nullptr; curNodePtr = curNodePtr->next) {
        if (!IsValidNode(*curNodePtr)) {
            WLOGFE("[WmConfig]: invalid node!");
            continue;
        }

        auto nodeName = curNodePtr->name;
        if (!xmlStrcmp(nodeName, reinterpret_cast<const xmlChar*>("decor")) ||
            !xmlStrcmp(nodeName, reinterpret_cast<const xmlChar*>("minimizeByOther"))) {
            ReadEnableConfigInfo(curNodePtr);
            continue;
        }
        if (!xmlStrcmp(nodeName, reinterpret_cast<const xmlChar*>("maxAppWindowNumber")) ||
            !xmlStrcmp(nodeName, reinterpret_cast<const xmlChar*>("modeChangeHotZones"))) {
            ReadNumbersConfigInfo(curNodePtr);
            continue;
        }
    }
    xmlFreeDoc(docPtr);
    return true;
}

bool WindowManagerConfig::IsValidNode(const xmlNode& currNode)
{
    if (currNode.name == nullptr || currNode.type == XML_COMMENT_NODE) {
        return false;
    }
    return true;
}

void WindowManagerConfig::ReadEnableConfigInfo(const xmlNodePtr& currNode)
{
    xmlChar* enable = xmlGetProp(currNode, reinterpret_cast<const xmlChar*>("enable"));
    if (enable == nullptr) {
        WLOGFE("[WmConfig] read xml node error: nodeName:(%{public}s)", currNode->name);
        return;
    }

    std::string nodeName = reinterpret_cast<const char *>(currNode->name);
    enableConfig_[nodeName] = xmlStrcmp(enable, reinterpret_cast<const xmlChar*>("true")) ? false : true;
    xmlFree(enable);
}

void WindowManagerConfig::ReadNumbersConfigInfo(const xmlNodePtr& currNode)
{
    xmlChar* context = xmlNodeGetContent(currNode);
    if (context == nullptr) {
        WLOGFE("[WmConfig] read xml node error: nodeName:(%{public}s)", currNode->name);
        return;
    }

    std::vector<int> numbersVec;
    std::string numbersStr = reinterpret_cast<const char*>(context);
    auto numbers = WindowHelper::Split(numbersStr, " ");
    for (auto& num : numbers) {
        if (!WindowHelper::IsNumber(num)) {
            WLOGFE("[WmConfig] read number error: nodeName:(%{public}s)", currNode->name);
            xmlFree(context);
            return;
        }
        numbersVec.emplace_back(std::stoi(num));
    }

    std::string nodeName = reinterpret_cast<const char *>(currNode->name);
    numbersConfig_[nodeName] = numbersVec;
    xmlFree(context);
}

const std::map<std::string, bool>& WindowManagerConfig::GetEnableConfig()
{
    return enableConfig_;
}

const std::map<std::string, std::vector<int>>& WindowManagerConfig::GetNumbersConfig()
{
    return numbersConfig_;
}

void WindowManagerConfig::DumpConfig()
{
    for (auto& enable : enableConfig_) {
        WLOGFI("[WmConfig] Enable: %{public}s %{public}u", enable.first.c_str(), enable.second);
    }

    for (auto& numbers : numbersConfig_) {
        WLOGFI("[WmConfig] Numbers: %{public}s %{public}zu", numbers.first.c_str(), numbers.second.size());
        for (auto& num : numbers.second) {
            WLOGFI("[WmConfig] Num: %{public}d", num);
        }
    }
}
} // namespace Rosen
} // namespace OHOS
