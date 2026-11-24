package com.green;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.util.function.Consumer;

public class MessageHandler implements Runnable
{
    private static volatile Consumer<JsonObject> lobbyEventHandler;

    public static void setLobbyEventHandler(Consumer<JsonObject> handler)
    {
        lobbyEventHandler = handler;
    }

    @Override
    public void run()
    {
        while (!Thread.currentThread().isInterrupted())
        {
            try 
            {
            String message = MessageBridge.getInQueue().take();
            handleMessage(message);
            }
            catch (InterruptedException e)
            {
                Thread.currentThread().interrupt();
                break;
            }
        }
    }

    private void handleMessage(String message)
    {
        try 
        {
            JsonObject json = JsonParser.parseString(message).getAsJsonObject();
            String command = json.get("command").getAsString();
            
            if (json.has("requestId"))
            {
                int requestId = json.get("requestId").getAsInt();
                if (requestId > 0)
                {
                    RequestManager.handleResponse(requestId, json);
                    return;
                }
            }
            switch (command)
            {
                case "game_started":
                case "player_joined":
                case "player_left":
                case "host_left":
                    Consumer<JsonObject> handler = lobbyEventHandler;
                    if (handler != null)
                    {
                        handler.accept(json);
                    }
                    break;
                
                default:
                    System.out.println("Unknown command: " + command);
                    break;
            }
        } 
        catch (Exception e) 
        {
            System.out.println("Invalid server message: " + e.getMessage());
        }
    }
}
