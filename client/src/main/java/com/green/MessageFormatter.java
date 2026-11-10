package com.green;

import com.google.gson.JsonObject;

public class MessageFormatter 
{
    public static String login(String username, String password)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "LOGIN");
        json.addProperty("username", username);
        json.addProperty("password", password);
        return json.toString();
    }

    public static String createAccount(String email, String username, String password)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "CREATE_ACCOUNT");
        json.addProperty("email", email);
        json.addProperty("username", username);
        json.addProperty("password", password);
        return json.toString();
    }
}
