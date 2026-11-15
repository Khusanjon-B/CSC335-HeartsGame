package com.green;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

public class MessageHandler implements Runnable
{
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
                String requestId = json.get("requestId").getAsString();
                RequestManager.handleResponse(requestId, json);
                return;
            }
            switch (command)
            {
                case "game_started":
                    // Handle game started event
                    break;
                case "player_joined":
                    // Handle player joined event
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
