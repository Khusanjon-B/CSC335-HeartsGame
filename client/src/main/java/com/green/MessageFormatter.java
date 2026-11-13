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
        json.addProperty("command", "CREATE_ACCOUNT");
        json.addProperty("email", email);
        json.addProperty("username", username);
        json.addProperty("password", password);
        return json.toString();
    }

    private static boolean validPassword(String password)
    {
        if (password.length() <= 8) return false;

        boolean hasLetter = password.matches(".*[a-zA-Z].*");
        boolean hasNumber = password.matches(".*[0-9].*");
        boolean hasSpecial = password.matches(".*[!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>\\/?].*");
        return hasLetter && hasNumber && hasSpecial;
    }
    private static boolean validEmail(String email)
    {
        return email.matches("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    }

    private static boolean validUsername(String username)
{
    return username != null && username.matches("^[a-zA-Z0-9_-]+$");
}

    public static String recoverPassword(String input)
    {
        JsonObject json = new JsonObject();
        json.addProperty("command", "recoverPassword");
        if (input.contains(".") && input.contains("@"))
        {
            json.addProperty("email", input);
        }
        else
        {
            json.addProperty("username", input);
        }
        
        return json.toString();
    }
}
