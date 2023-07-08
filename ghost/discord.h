/*

   Copyright [2008] [Trevor Hogan]
   Copyright [2022] [Sam Leung]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

   CODE PORTED FROM THE ORIGINAL GHOST PROJECT: http://ghost.pwner.org/

*/

#ifndef DISCORD_H
#define DISCORD_H

#include <dpp/dpp.h>

class CDiscord
{
public:
    CDiscord(CGHost *nGHost, string nToken, uint64_t nChannelId);
    void SendChat(string message);

protected:
    std::unique_ptr<dpp::cluster> m_DiscordBot;
    CGHost *m_GHost;
    dpp::snowflake m_ChannelId;
    bool m_Exiting;
    vector<PairedAdminAdd> m_PairedAdminAdds;
    vector<PairedAdminCount> m_PairedAdminCounts;
    vector<PairedAdminRemove> m_PairedAdminRemoves;
    vector<PairedBanCount> m_PairedBanCounts;
    vector<PairedBanRemove> m_PairedBanRemoves;
    std::string m_PlayerName;
    void EventBotCommand(string command, string payload);

private:
    std::function<void(const dpp::log_t &)> DppLogger();
};

#endif