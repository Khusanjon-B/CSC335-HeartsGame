package com.green;

import com.google.gson.JsonObject;

public class MessageFormatter 
{
    public static String login(String username, String password)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "login");
        json.addProperty("username", username);
        json.addProperty("password", password);
        return json.toString();
    }

    public static String createAccount(String email, String username, String password)
    {
        if (!validPassword(password))
        {
            throw new IllegalArgumentException("Password does not meet the requirements");
        }
        if (!validEmail(email))
        {
            throw new IllegalArgumentException("Invalid Email Format");
        }
        if (!validUsername(username))
        {
            throw new IllegalArgumentException("Invalid Username Format");
        }

        JsonObject json = new JsonObject();
        json.addProperty("command", "register");
        json.addProperty("email", email);
        json.addProperty("username", username);
        json.addProperty("password", password);
        return json.toString();
    }
}
