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

#include "ghost.h"
#include "util.h"
#include "config.h"
#include "language.h"
#include "socket.h"
#include "ghostdb.h"
#include "bnet.h"
#include "map.h"
#include "packed.h"
#include "savegame.h"
#include "replay.h"
#include "gameplayer.h"
#include "gameprotocol.h"
#include "game_base.h"
#include "discord.h"

#include <string.h>

#include <boost/filesystem.hpp>

#include <dpp/dpp.h>

using namespace boost::filesystem;

CDiscord::CDiscord(CGHost *nGHost, string nToken, uint64_t nChannelId)
{
  m_GHost = nGHost;

  m_DiscordBot = std::make_unique<dpp::cluster>(nToken, dpp::i_default_intents | dpp::i_message_content);
  m_ChannelId = nChannelId;

  m_PlayerName = "";

  m_DiscordBot->on_log(DppLogger());

  m_DiscordBot->on_message_create([&](const dpp::message_create_t &event)
                                  {
    if (event.msg.channel_id != m_ChannelId) {
      return;
    }

    std::stringstream ss(event.msg.content);
    std::string command = "";
    std::string payload = "";

    ss >> command;

    if (command.length() <= 2) {
      return;
    }

    if (command.at(0) != '!') {
      return;
    }

    std::size_t payload_start_pos = event.msg.content.find(" ");
    if (payload_start_pos != std::string::npos && payload_start_pos + 1 < event.msg.content.length()) {
      payload = event.msg.content.substr(payload_start_pos + 1);
    }

    command = command.substr(1);

    if (command == "player") {
      m_PlayerName = payload;
      m_DiscordBot->message_create(dpp::message(event.msg.channel_id, "Username set: \"" + m_PlayerName + "\""));
    }

    if (m_PlayerName.length() < 1) {
      m_DiscordBot->message_create(dpp::message(event.msg.channel_id, "Please set your username.\nCommand: !player <name>"));
      return;
    }

    EventBotCommand(command, payload); });

  m_DiscordBot->start(true);
}

void CDiscord::SendChat(string message)
{
  m_DiscordBot->message_create(dpp::message(m_ChannelId, message));
}

std::function<void(const dpp::log_t &)> CDiscord::DppLogger()
{
  return [&](const dpp::log_t &event)
  {
    if (event.severity > dpp::ll_trace)
    {
      CONSOLE_Print("[DISCORD] " + dpp::utility::loglevel(event.severity) + ": " + event.message);
    }
  };
}

void CDiscord::EventBotCommand(string command, string payload)
{
  string User = m_PlayerName;
  string Command = command;
  string Payload = payload;
  string player = m_PlayerName;

  CONSOLE_Print("[DISCORD] admin [" + User + "] sent command [" + Command + "] with payload [" + Payload + "]");

  /*****************
   * ADMIN COMMANDS *
   ******************/

  //
  // !ADDADMIN
  //

  if (Command == "addadmin" && !Payload.empty())
  {
    // extract the name and the server
    // e.g. "Varlock useast.battle.net" -> name: "Varlock", server: "useast.battle.net"

    string Name;
    string Server;
    stringstream SS;
    SS << Payload;
    SS >> Name;

    if (SS.eof())
    {
      if (m_GHost->m_BNETs.size() == 1)
        Server = m_GHost->m_BNETs[0]->GetServer();
      else
        CONSOLE_Print("[DISCORD] missing input #2 to addadmin command");
    }
    else
      SS >> Server;

    if (!Server.empty())
    {
      string Servers;
      bool FoundServer = false;

      for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
      {
        if (Servers.empty())
          Servers = (*i)->GetServer();
        else
          Servers += ", " + (*i)->GetServer();

        if ((*i)->GetServer() == Server)
        {
          FoundServer = true;

          if ((*i)->IsAdmin(Name))
            SendChat(m_GHost->m_Language->UserIsAlreadyAnAdmin(Server, Name));
          else
            m_PairedAdminAdds.push_back(PairedAdminAdd(player, m_GHost->m_DB->ThreadedAdminAdd(Server, Name)));

          break;
        }
      }

      if (!FoundServer)
        SendChat(m_GHost->m_Language->ValidServers(Servers));
    }
  }

  //
  // !AUTOHOST
  //

  else if (Command == "autohost")
  {
    if (Payload.empty() || Payload == "off")
    {
      SendChat(m_GHost->m_Language->AutoHostDisabled());
      m_GHost->m_AutoHostGameName.clear();
      m_GHost->m_AutoHostOwner.clear();
      m_GHost->m_AutoHostServer.clear();
      m_GHost->m_AutoHostMaximumGames = 0;
      m_GHost->m_AutoHostAutoStartPlayers = 0;
      m_GHost->m_LastAutoHostTime = GetTime();
      m_GHost->m_AutoHostMatchMaking = false;
      m_GHost->m_AutoHostMinimumScore = 0.0;
      m_GHost->m_AutoHostMaximumScore = 0.0;
    }
    else
    {
      // extract the maximum games, auto start players, and the game name
      // e.g. "5 10 BattleShips Pro" -> maximum games: "5", auto start players: "10", game name: "BattleShips Pro"

      uint32_t MaximumGames;
      uint32_t AutoStartPlayers;
      string GameName;
      stringstream SS;
      SS << Payload;
      SS >> MaximumGames;

      if (SS.fail() || MaximumGames == 0)
        CONSOLE_Print("[DISCORD] bad input #1 to autohost command");
      else
      {
        SS >> AutoStartPlayers;

        if (SS.fail() || AutoStartPlayers == 0)
          CONSOLE_Print("[DISCORD] bad input #2 to autohost command");
        else
        {
          if (SS.eof())
            CONSOLE_Print("[DISCORD] missing input #3 to autohost command");
          else
          {
            getline(SS, GameName);
            string ::size_type Start = GameName.find_first_not_of(" ");

            if (Start != string ::npos)
              GameName = GameName.substr(Start);

            SendChat(m_GHost->m_Language->AutoHostEnabled());
            delete m_GHost->m_AutoHostMap;
            m_GHost->m_AutoHostMap = new CMap(*m_GHost->m_Map);
            m_GHost->m_AutoHostGameName = GameName;
            m_GHost->m_AutoHostOwner = User;
            m_GHost->m_AutoHostServer.clear();
            m_GHost->m_AutoHostMaximumGames = MaximumGames;
            m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
            m_GHost->m_LastAutoHostTime = GetTime();
            m_GHost->m_AutoHostMatchMaking = false;
            m_GHost->m_AutoHostMinimumScore = 0.0;
            m_GHost->m_AutoHostMaximumScore = 0.0;
          }
        }
      }
    }
  }

  //
  // !AUTOHOSTMM
  //

  else if (Command == "autohostmm")
  {
    if (Payload.empty() || Payload == "off")
    {
      SendChat(m_GHost->m_Language->AutoHostDisabled());
      m_GHost->m_AutoHostGameName.clear();
      m_GHost->m_AutoHostOwner.clear();
      m_GHost->m_AutoHostServer.clear();
      m_GHost->m_AutoHostMaximumGames = 0;
      m_GHost->m_AutoHostAutoStartPlayers = 0;
      m_GHost->m_LastAutoHostTime = GetTime();
      m_GHost->m_AutoHostMatchMaking = false;
      m_GHost->m_AutoHostMinimumScore = 0.0;
      m_GHost->m_AutoHostMaximumScore = 0.0;
    }
    else
    {
      // extract the maximum games, auto start players, and the game name
      // e.g. "5 10 800 1200 BattleShips Pro" -> maximum games: "5", auto start players: "10", minimum score: "800", maximum score: "1200", game name: "BattleShips Pro"

      uint32_t MaximumGames;
      uint32_t AutoStartPlayers;
      double MinimumScore;
      double MaximumScore;
      string GameName;
      stringstream SS;
      SS << Payload;
      SS >> MaximumGames;

      if (SS.fail() || MaximumGames == 0)
        CONSOLE_Print("[DISCORD] bad input #1 to autohostmm command");
      else
      {
        SS >> AutoStartPlayers;

        if (SS.fail() || AutoStartPlayers == 0)
          CONSOLE_Print("[DISCORD] bad input #2 to autohostmm command");
        else
        {
          SS >> MinimumScore;

          if (SS.fail())
            CONSOLE_Print("[DISCORD] bad input #3 to autohostmm command");
          else
          {
            SS >> MaximumScore;

            if (SS.fail())
              CONSOLE_Print("[DISCORD] bad input #4 to autohostmm command");
            else
            {
              if (SS.eof())
                CONSOLE_Print("[DISCORD] missing input #5 to autohostmm command");
              else
              {
                getline(SS, GameName);
                string ::size_type Start = GameName.find_first_not_of(" ");

                if (Start != string ::npos)
                  GameName = GameName.substr(Start);

                SendChat(m_GHost->m_Language->AutoHostEnabled());
                delete m_GHost->m_AutoHostMap;
                m_GHost->m_AutoHostMap = new CMap(*m_GHost->m_Map);
                m_GHost->m_AutoHostGameName = GameName;
                m_GHost->m_AutoHostOwner = User;
                m_GHost->m_AutoHostServer.clear();
                m_GHost->m_AutoHostMaximumGames = MaximumGames;
                m_GHost->m_AutoHostAutoStartPlayers = AutoStartPlayers;
                m_GHost->m_LastAutoHostTime = GetTime();
                m_GHost->m_AutoHostMatchMaking = true;
                m_GHost->m_AutoHostMinimumScore = MinimumScore;
                m_GHost->m_AutoHostMaximumScore = MaximumScore;
              }
            }
          }
        }
      }
    }
  }

  //
  // !CHECKADMIN
  //

  else if (Command == "checkadmin" && !Payload.empty())
  {
    // extract the name and the server
    // e.g. "Varlock useast.battle.net" -> name: "Varlock", server: "useast.battle.net"

    string Name;
    string Server;
    stringstream SS;
    SS << Payload;
    SS >> Name;

    if (SS.eof())
    {
      if (m_GHost->m_BNETs.size() == 1)
        Server = m_GHost->m_BNETs[0]->GetServer();
      else
        CONSOLE_Print("[DISCORD] missing input #2 to checkadmin command");
    }
    else
      SS >> Server;

    if (!Server.empty())
    {
      string Servers;
      bool FoundServer = false;

      for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
      {
        if (Servers.empty())
          Servers = (*i)->GetServer();
        else
          Servers += ", " + (*i)->GetServer();

        if ((*i)->GetServer() == Server)
        {
          FoundServer = true;

          if ((*i)->IsAdmin(Name))
            SendChat(m_GHost->m_Language->UserIsAnAdmin(Server, Name));
          else
            SendChat(m_GHost->m_Language->UserIsNotAnAdmin(Server, Name));

          break;
        }
      }

      if (!FoundServer)
        SendChat(m_GHost->m_Language->ValidServers(Servers));
    }
  }

  //
  // !CHECKBAN
  //

  else if (Command == "checkban" && !Payload.empty())
  {
    // extract the name and the server
    // e.g. "Varlock useast.battle.net" -> name: "Varlock", server: "useast.battle.net"

    string Name;
    string Server;
    stringstream SS;
    SS << Payload;
    SS >> Name;

    if (SS.eof())
    {
      if (m_GHost->m_BNETs.size() == 1)
        Server = m_GHost->m_BNETs[0]->GetServer();
      else
        CONSOLE_Print("[DISCORD] missing input #2 to checkban command");
    }
    else
      SS >> Server;

    if (!Server.empty())
    {
      string Servers;
      bool FoundServer = false;

      for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
      {
        if (Servers.empty())
          Servers = (*i)->GetServer();
        else
          Servers += ", " + (*i)->GetServer();

        if ((*i)->GetServer() == Server)
        {
          FoundServer = true;
          CDBBan *Ban = (*i)->IsBannedName(Name);

          if (Ban)
            SendChat(m_GHost->m_Language->UserWasBannedOnByBecause(Server, Name, Ban->GetDate(), Ban->GetAdmin(), Ban->GetReason()));
          else
            SendChat(m_GHost->m_Language->UserIsNotBanned(Server, Name));

          break;
        }
      }

      if (!FoundServer)
        SendChat(m_GHost->m_Language->ValidServers(Servers));
    }
  }

  //
  // !COUNTADMINS
  //

  else if (Command == "countadmins")
  {
    string Server = Payload;

    if (Server.empty() && m_GHost->m_BNETs.size() == 1)
      Server = m_GHost->m_BNETs[0]->GetServer();

    if (!Server.empty())
      m_PairedAdminCounts.push_back(PairedAdminCount(player, m_GHost->m_DB->ThreadedAdminCount(Server)));
  }

  //
  // !COUNTBANS
  //

  else if (Command == "countbans")
  {
    string Server = Payload;

    if (Server.empty() && m_GHost->m_BNETs.size() == 1)
      Server = m_GHost->m_BNETs[0]->GetServer();

    if (!Server.empty())
      m_PairedBanCounts.push_back(PairedBanCount(player, m_GHost->m_DB->ThreadedBanCount(Server)));
  }

  //
  // !DELADMIN
  //

  else if (Command == "deladmin" && !Payload.empty())
  {
    // extract the name and the server
    // e.g. "Varlock useast.battle.net" -> name: "Varlock", server: "useast.battle.net"

    string Name;
    string Server;
    stringstream SS;
    SS << Payload;
    SS >> Name;

    if (SS.eof())
    {
      if (m_GHost->m_BNETs.size() == 1)
        Server = m_GHost->m_BNETs[0]->GetServer();
      else
        CONSOLE_Print("[DISCORD] missing input #2 to deladmin command");
    }
    else
      SS >> Server;

    if (!Server.empty())
    {
      string Servers;
      bool FoundServer = false;

      for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
      {
        if (Servers.empty())
          Servers = (*i)->GetServer();
        else
          Servers += ", " + (*i)->GetServer();

        if ((*i)->GetServer() == Server)
        {
          FoundServer = true;

          if (!(*i)->IsAdmin(Name))
            SendChat(m_GHost->m_Language->UserIsNotAnAdmin(Server, Name));
          else
            m_PairedAdminRemoves.push_back(PairedAdminRemove(player, m_GHost->m_DB->ThreadedAdminRemove(Server, Name)));

          break;
        }
      }

      if (!FoundServer)
        SendChat(m_GHost->m_Language->ValidServers(Servers));
    }
  }

  //
  // !DELBAN
  // !UNBAN
  //

  else if ((Command == "delban" || Command == "unban") && !Payload.empty())
    m_PairedBanRemoves.push_back(PairedBanRemove(player, m_GHost->m_DB->ThreadedBanRemove(Payload)));

  //
  // !DISABLE
  //

  else if (Command == "disable")
  {
    SendChat(m_GHost->m_Language->BotDisabled());
    m_GHost->m_Enabled = false;
  }

  //
  // !DOWNLOADS
  //

  else if (Command == "downloads" && !Payload.empty())
  {
    uint32_t Downloads = UTIL_ToUInt32(Payload);

    if (Downloads == 0)
    {
      SendChat(m_GHost->m_Language->MapDownloadsDisabled());
      m_GHost->m_AllowDownloads = 0;
    }
    else if (Downloads == 1)
    {
      SendChat(m_GHost->m_Language->MapDownloadsEnabled());
      m_GHost->m_AllowDownloads = 1;
    }
    else if (Downloads == 2)
    {
      SendChat(m_GHost->m_Language->MapDownloadsConditional());
      m_GHost->m_AllowDownloads = 2;
    }
  }

  //
  // !ENABLE
  //

  else if (Command == "enable")
  {
    SendChat(m_GHost->m_Language->BotEnabled());
    m_GHost->m_Enabled = true;
  }

  //
  // !END
  //

  else if (Command == "end" && !Payload.empty())
  {
    // todotodo: what if a game ends just as you're typing this command and the numbering changes?

    uint32_t GameNumber = UTIL_ToUInt32(Payload) - 1;

    if (GameNumber < m_GHost->m_Games.size())
    {
      SendChat(m_GHost->m_Language->EndingGame(m_GHost->m_Games[GameNumber]->GetDescription()));
      CONSOLE_Print("[GAME: " + m_GHost->m_Games[GameNumber]->GetGameName() + "] is over (admin ended game)");
      m_GHost->m_Games[GameNumber]->StopPlayers("was disconnected (admin ended game)");
    }
    else
      SendChat(m_GHost->m_Language->GameNumberDoesntExist(Payload));
  }

  //
  // !ENFORCESG
  //

  else if (Command == "enforcesg" && !Payload.empty())
  {
    // only load files in the current directory just to be safe

    if (Payload.find("/") != string ::npos || Payload.find("\\") != string ::npos)
      SendChat(m_GHost->m_Language->UnableToLoadReplaysOutside());
    else
    {
      string File = m_GHost->m_ReplayPath + Payload + ".w3g";

      if (UTIL_FileExists(File))
      {
        SendChat(m_GHost->m_Language->LoadingReplay(File));
        CReplay *Replay = new CReplay();
        Replay->Load(File, false);
        Replay->ParseReplay(false);
        m_GHost->m_EnforcePlayers = Replay->GetPlayers();
        delete Replay;
      }
      else
        SendChat(m_GHost->m_Language->UnableToLoadReplayDoesntExist(File));
    }
  }

  //
  // !EXIT
  // !QUIT
  //

  else if (Command == "exit" || Command == "quit")
  {
    if (Payload == "nice")
      m_GHost->m_ExitingNice = true;
    else if (Payload == "force")
      m_Exiting = true;
    else
    {
      if (m_GHost->m_CurrentGame || !m_GHost->m_Games.empty())
        SendChat(m_GHost->m_Language->AtLeastOneGameActiveUseForceToShutdown());
      else
        m_Exiting = true;
    }
  }

  //
  // !GETGAME
  //

  else if (Command == "getgame" && !Payload.empty())
  {
    uint32_t GameNumber = UTIL_ToUInt32(Payload) - 1;

    if (GameNumber < m_GHost->m_Games.size())
      SendChat(m_GHost->m_Language->GameNumberIs(Payload, m_GHost->m_Games[GameNumber]->GetDescription()));
    else
      SendChat(m_GHost->m_Language->GameNumberDoesntExist(Payload));
  }

  //
  // !GETGAMES
  //

  else if (Command == "getgames")
  {
    if (m_GHost->m_CurrentGame)
      SendChat(m_GHost->m_Language->GameIsInTheLobby(m_GHost->m_CurrentGame->GetDescription(), UTIL_ToString(m_GHost->m_Games.size()), UTIL_ToString(m_GHost->m_MaxGames)));
    else
      SendChat(m_GHost->m_Language->ThereIsNoGameInTheLobby(UTIL_ToString(m_GHost->m_Games.size()), UTIL_ToString(m_GHost->m_MaxGames)));
  }

  //
  // !HOSTSG
  //

  else if (Command == "hostsg" && !Payload.empty())
    m_GHost->CreateGame(m_GHost->m_Map, GAME_PRIVATE, true, Payload, User, User, string(), false);

  //
  // !LOAD (load config file)
  //

  else if (Command == "load")
  {
    if (Payload.empty())
      SendChat(m_GHost->m_Language->CurrentlyLoadedMapCFGIs(m_GHost->m_Map->GetCFGFile()));
    else
    {
      string FoundMapConfigs;

      try
      {
        path MapCFGPath(m_GHost->m_MapCFGPath);
        string Pattern = Payload;
        transform(Pattern.begin(), Pattern.end(), Pattern.begin(), (int (*)(int))tolower);

        if (!exists(MapCFGPath))
        {
          CONSOLE_Print("[DISCORD] error listing map configs - map config path doesn't exist");
          SendChat(m_GHost->m_Language->ErrorListingMapConfigs());
        }
        else
        {
          directory_iterator EndIterator;
          path LastMatch;
          uint32_t Matches = 0;

          for (directory_iterator i(MapCFGPath); i != EndIterator; ++i)
          {
            string FileName = i->path().filename().string();
            string Stem = i->path().stem().string();
            transform(FileName.begin(), FileName.end(), FileName.begin(), (int (*)(int))tolower);
            transform(Stem.begin(), Stem.end(), Stem.begin(), (int (*)(int))tolower);

            if (!is_directory(i->status()) && i->path().extension() == ".cfg" && FileName.find(Pattern) != string ::npos)
            {
              LastMatch = i->path();
              ++Matches;

              if (FoundMapConfigs.empty())
                FoundMapConfigs = i->path().filename().string();
              else
                FoundMapConfigs += ", " + i->path().filename().string();

              // if the pattern matches the filename exactly, with or without extension, stop any further matching

              if (FileName == Pattern || Stem == Pattern)
              {
                Matches = 1;
                break;
              }
            }
          }

          if (Matches == 0)
            SendChat(m_GHost->m_Language->NoMapConfigsFound());
          else if (Matches == 1)
          {
            string File = LastMatch.filename().string();
            SendChat(m_GHost->m_Language->LoadingConfigFile(m_GHost->m_MapCFGPath + File));
            CConfig MapCFG;
            MapCFG.Read(LastMatch.string());
            m_GHost->m_Map->Load(&MapCFG, m_GHost->m_MapCFGPath + File);
          }
          else
            SendChat(m_GHost->m_Language->FoundMapConfigs(FoundMapConfigs));
        }
      }
      catch (const exception &ex)
      {
        CONSOLE_Print(string("[DISCORD] error listing map configs - caught exception [") + ex.what() + "]");
        SendChat(m_GHost->m_Language->ErrorListingMapConfigs());
      }
    }
  }

  //
  // !LOADSG
  //

  else if (Command == "loadsg" && !Payload.empty())
  {
    // only load files in the current directory just to be safe

    if (Payload.find("/") != string ::npos || Payload.find("\\") != string ::npos)
      SendChat(m_GHost->m_Language->UnableToLoadSaveGamesOutside());
    else
    {
      string File = m_GHost->m_SaveGamePath + Payload + ".w3z";
      string FileNoPath = Payload + ".w3z";

      if (UTIL_FileExists(File))
      {
        if (m_GHost->m_CurrentGame)
          SendChat(m_GHost->m_Language->UnableToLoadSaveGameGameInLobby());
        else
        {
          SendChat(m_GHost->m_Language->LoadingSaveGame(File));
          m_GHost->m_SaveGame->Load(File, false);
          m_GHost->m_SaveGame->ParseSaveGame();
          m_GHost->m_SaveGame->SetFileName(File);
          m_GHost->m_SaveGame->SetFileNameNoPath(FileNoPath);
        }
      }
      else
        SendChat(m_GHost->m_Language->UnableToLoadSaveGameDoesntExist(File));
    }
  }

  //
  // !MAP (load map file)
  //

  else if (Command == "map")
  {
    if (Payload.empty())
      SendChat(m_GHost->m_Language->CurrentlyLoadedMapCFGIs(m_GHost->m_Map->GetCFGFile()));
    else
    {
      string FoundMaps;

      try
      {
        path MapPath(m_GHost->m_MapPath);
        string Pattern = Payload;
        transform(Pattern.begin(), Pattern.end(), Pattern.begin(), (int (*)(int))tolower);

        if (!exists(MapPath))
        {
          CONSOLE_Print("[DISCORD] error listing maps - map path doesn't exist");
          SendChat(m_GHost->m_Language->ErrorListingMaps());
        }
        else
        {
          directory_iterator EndIterator;
          path LastMatch;
          uint32_t Matches = 0;

          for (directory_iterator i(MapPath); i != EndIterator; ++i)
          {
            string FileName = i->path().filename().string();
            string Stem = i->path().stem().string();
            transform(FileName.begin(), FileName.end(), FileName.begin(), (int (*)(int))tolower);
            transform(Stem.begin(), Stem.end(), Stem.begin(), (int (*)(int))tolower);

            if (!is_directory(i->status()) && FileName.find(Pattern) != string ::npos)
            {
              LastMatch = i->path();
              ++Matches;

              if (FoundMaps.empty())
                FoundMaps = i->path().filename().string();
              else
                FoundMaps += ", " + i->path().filename().string();

              // if the pattern matches the filename exactly, with or without extension, stop any further matching

              if (FileName == Pattern || Stem == Pattern)
              {
                Matches = 1;
                break;
              }
            }
          }

          if (Matches == 0)
            SendChat(m_GHost->m_Language->NoMapsFound());
          else if (Matches == 1)
          {
            string File = LastMatch.filename().string();
            SendChat(m_GHost->m_Language->LoadingConfigFile(File));

            // hackhack: create a config file in memory with the required information to load the map

            CConfig MapCFG;
            MapCFG.Set("map_path", "Maps\\Download\\" + File);
            MapCFG.Set("map_localpath", File);
            m_GHost->m_Map->Load(&MapCFG, File);
          }
          else
            SendChat(m_GHost->m_Language->FoundMaps(FoundMaps));
        }
      }
      catch (const exception &ex)
      {
        CONSOLE_Print(string("[DISCORD] error listing maps - caught exception [") + ex.what() + "]");
        SendChat(m_GHost->m_Language->ErrorListingMaps());
      }
    }
  }

  //
  // !PRIV (host private game)
  //

  else if (Command == "priv" && !Payload.empty())
    m_GHost->CreateGame(m_GHost->m_Map, GAME_PRIVATE, false, Payload, User, User, string(), false);

  //
  // !PRIVBY (host private game by other player)
  //

  else if (Command == "privby" && !Payload.empty())
  {
    // extract the owner and the game name
    // e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

    string Owner;
    string GameName;
    string ::size_type GameNameStart = Payload.find(" ");

    if (GameNameStart != string ::npos)
    {
      Owner = Payload.substr(0, GameNameStart);
      GameName = Payload.substr(GameNameStart + 1);
      m_GHost->CreateGame(m_GHost->m_Map, GAME_PRIVATE, false, GameName, Owner, User, string(), false);
    }
  }

  //
  // !PUB (host public game)
  //

  else if (Command == "pub" && !Payload.empty())
    m_GHost->CreateGame(m_GHost->m_Map, GAME_PUBLIC, false, Payload, User, User, string(), false);

  //
  // !PUBBY (host public game by other player)
  //

  if (Command == "pubby" && !Payload.empty())
  {
    // extract the owner and the game name
    // e.g. "Varlock dota 6.54b arem ~~~" -> owner: "Varlock", game name: "dota 6.54b arem ~~~"

    string Owner;
    string GameName;
    string ::size_type GameNameStart = Payload.find(" ");

    if (GameNameStart != string ::npos)
    {
      Owner = Payload.substr(0, GameNameStart);
      GameName = Payload.substr(GameNameStart + 1);
      m_GHost->CreateGame(m_GHost->m_Map, GAME_PUBLIC, false, GameName, Owner, User, string(), false);
    }
  }

  //
  // !RELOAD
  //

  else if (Command == "reload")
  {
    SendChat(m_GHost->m_Language->ReloadingConfigurationFiles());
    m_GHost->ReloadConfigs();
  }

  //
  // !SAY
  //

  else if (Command == "say" && !Payload.empty())
  {
    for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
      (*i)->QueueChatCommand(Payload);
  }

  //
  // !SAYGAME
  //

  else if (Command == "saygame" && !Payload.empty())
  {
    // extract the game number and the message
    // e.g. "3 hello everyone" -> game number: "3", message: "hello everyone"

    uint32_t GameNumber;
    string Message;
    stringstream SS;
    SS << Payload;
    SS >> GameNumber;

    if (SS.fail())
      CONSOLE_Print("[DISCORD] bad input #1 to saygame command");
    else
    {
      if (SS.eof())
        CONSOLE_Print("[DISCORD] missing input #2 to saygame command");
      else
      {
        getline(SS, Message);
        string ::size_type Start = Message.find_first_not_of(" ");

        if (Start != string ::npos)
          Message = Message.substr(Start);

        if (GameNumber - 1 < m_GHost->m_Games.size())
          m_GHost->m_Games[GameNumber - 1]->SendAllChat("ADMIN: " + Message);
        else
          SendChat(m_GHost->m_Language->GameNumberDoesntExist(UTIL_ToString(GameNumber)));
      }
    }
  }

  //
  // !SAYGAMES
  //

  else if (Command == "saygames" && !Payload.empty())
  {
    if (m_GHost->m_CurrentGame)
      m_GHost->m_CurrentGame->SendAllChat(Payload);

    for (vector<CBaseGame *>::iterator i = m_GHost->m_Games.begin(); i != m_GHost->m_Games.end(); ++i)
      (*i)->SendAllChat("ADMIN: " + Payload);
  }

  //
  // !UNHOST
  //

  else if (Command == "unhost")
  {
    if (m_GHost->m_CurrentGame)
    {
      if (m_GHost->m_CurrentGame->GetCountDownStarted())
        SendChat(m_GHost->m_Language->UnableToUnhostGameCountdownStarted(m_GHost->m_CurrentGame->GetDescription()));
      else
      {
        SendChat(m_GHost->m_Language->UnhostingGame(m_GHost->m_CurrentGame->GetDescription()));
        m_GHost->m_CurrentGame->SetExiting(true);
      }
    }
    else
      SendChat(m_GHost->m_Language->UnableToUnhostGameNoGameInLobby());
  }

  //
  // !W
  //

  else if (Command == "w" && !Payload.empty())
  {
    // extract the name and the message
    // e.g. "Varlock hello there!" -> name: "Varlock", message: "hello there!"

    string Name;
    string Message;
    string ::size_type MessageStart = Payload.find(" ");

    if (MessageStart != string ::npos)
    {
      Name = Payload.substr(0, MessageStart);
      Message = Payload.substr(MessageStart + 1);

      for (vector<CBNET *>::iterator i = m_GHost->m_BNETs.begin(); i != m_GHost->m_BNETs.end(); ++i)
        (*i)->QueueChatCommand(Message, Name, true);
    }
  }

  return;
}