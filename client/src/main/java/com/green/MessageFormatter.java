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

    public static String createAccount(String email, String username, String password, String pin)
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
        if (!validPin(pin))
        {
            throw new IllegalArgumentException("Invalid Pin Format");
        }

        JsonObject json = new JsonObject();
        json.addProperty("command", "register");
        json.addProperty("email", email);
        json.addProperty("username", username);
        json.addProperty("password", password);
        json.addProperty("pin", pin);
        return json.toString();

        
    }
    public static boolean validPassword(String password)
    {
        if (password == null || password.length() <= 8) return false;

        boolean hasLetter = password.matches(".*[a-zA-Z].*");
        boolean hasNumber = password.matches(".*[0-9].*");
        boolean hasSpecial = password.matches(".*[!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>\\/?].*");
        return hasLetter && hasNumber && hasSpecial;
    }
    public static boolean validEmail(String email)
    {
        return email.matches("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    }

    public static boolean validUsername(String username)
{
    return username != null && username.matches("^[a-zA-Z0-9_-]+$");
}

    public static boolean validPin(String pin)
    {
        return pin != null && pin.matches("[0-9]{4,8}"); // Adjust the range as needed
    }

    public static String recoverPassword(String username, String email, String pin, String newPassword)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "recover_password");
        json.addProperty("username", username);
        json.addProperty("email", email);
        json.addProperty("pin", pin);
        json.addProperty("newPassword", newPassword);
        return json.toString();
    }
    public static String logout(){
        JsonObject json = new JsonObject();
        json.addProperty("command", "logout");
        return json.toString();
    }

    public static String startLobby()
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "start_lobby");
        return json.toString();
    }

    public static String getLobbies()
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "get_lobbies");
        return json.toString();
    }

    public static String joinLobby(int lobbyId)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "join_lobby");
        json.addProperty("lobbyId", lobbyId);
        return json.toString();
    }

    public static String leaveLobby()
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "leave_lobby");
        return json.toString();
    }

    public static String startGame()
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "start_game");
        return json.toString();
    }
}