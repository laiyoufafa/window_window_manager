/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include "scene_board_judgement.h"

namespace OHOS::Rosen {
bool SceneBoardJudgement::IsSceneBoardEnabled()
{
    static bool isSceneBoardEnabled = false;
    static bool initialized = false;
    if (!initialized) {
        InitWithConfigFile(isSceneBoardEnabled);
        initialized = true;
    }
    return isSceneBoardEnabled;
}

std::ifstream& SceneBoardJudgement::SafeGetLine(std::ifstream& configFile, std::string& line)
{
    std::getline(configFile, line);
    if (line.size() && line[line.size() - 1] == '\r') {
        line = line.substr(0, line.size() - 1);
    }
    return configFile;
}

void SceneBoardJudgement::InitWithConfigFile(bool& isSceneBoardEnabled)
{
    std::ifstream configFile("/etc/windowscene.config");
    std::string line;
    if (configFile.is_open() && SafeGetLine(configFile, line) && line == "ENABLED") {
        isSceneBoardEnabled = true;
    }
    configFile.close();
}
} // namespace OHOS::Rosen