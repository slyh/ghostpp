Acquire Token
=============
1. Head to [Discord Developer Portal](https://discord.com/developers/applications) and create an application.
2. Select your newly created application, go to `Settings -> Bot -> Build-A-Bot -> Token` and click `Reset Token` to get the token. You need to fill it into `discord_bot_token` in your GHost++ config file.

Enable Permissions
==================
1. Go to `Settings -> Bot` and enable `Message Content Intent`.
2. Go to `Settings -> OAuth2 -> URL Generator`, tick `bot` in `Scopes`, `Read Messages/View Channels` and `Send Messages` in `Bot Permissions`.
3. Open the generated URL and add the application to your server.

After that, just add the bot into a channel. Then you can start issuing commands to GHost++ in that channel.

Channel ID
==========
When you are in a channel (using web browser), the URL shown in your browser will be something like: https://discord.com/channels/880729592419631645/830719694523636790

The last part (830719694523636790) is your current channel ID. Fill it into `discord_channel_id` in your GHost++ config file.

Usage
=====
Refer to `admin game lobby` part of the manual.

You need to set a player name before issuing any command.

Send `!player <name>` to set a player name. It will function similarly to the name of player connected to an admin game lobby.
