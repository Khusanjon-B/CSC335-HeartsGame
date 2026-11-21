package com.green;
import java.util.Scanner;
public class ConsoleManager 
{
    private Scanner scn = new Scanner(System.in); //scanner not close since that would also close system.in
    public void start() 
    {
        
        System.out.println("== Hearts Game ==");

        while (true)
        {
            Player currentPlayer = Player.getCurrentPlayer();
            if (currentPlayer != null)
            {
                System.out.println("Logged in as: " + currentPlayer.getUsername());
            }
            System.out.println("1. Login");
            System.out.println("2. Create Account");
            System.out.println("3. Exit");
            System.out.println("4. Recover Password");
            System.out.println("5. Logout");

            System.out.print("Enter your choice: ");

            String choice = scn.next();
            switch (choice) 
            {
                case "1":
                    System.out.println("Login selected");
                    login();
                    break;
                case "2":
                    System.out.println("Create Account selected");
                    createAccount();
                    break;
                case "3":
                    System.out.println("Exiting...");
                    return;
                case "4":
                    System.out.println("Recover Password selected");
                    recoverPassword();
                    break;
                case "5":
                    logout();
                    break;
                default:
                    System.out.println("Invalid choice, try again");
                    break;
            }
        }
    }
    private void logout()
    {
        if (Player.getCurrentPlayer() == null)
        {
            System.out.println("No player is currently logged in.");
            return;
        }

        RequestManager.requestLogout(response ->
            {
                if (response.has("success") && response.get("success").getAsBoolean())
                {
                    Player.resetPlayer();
                    System.out.println("Logout successful");
                }
                else
                {
                    String error = response.has("error")
                        ? response.get("error").getAsString() : "Unknown error";
                    System.out.println("Logout failed: " + error);
                }
            }
        );
    }
    private void login()
    {
        System.out.println("== Login ==");
        System.out.print("Username: ");
        String username = scn.next();
        System.out.print("Password: ");
        String password = scn.next();

        try
        {
            RequestManager.requestLogin(username, password, response ->
                {
                    boolean success = response.get("success").getAsBoolean();

                    if (success)
                    {
                        System.out.println("Login successful");
                        Player.setupPlayer(response.get("playerId").getAsInt(), username);
                    }
                    else
                    {
                        String error = response.has("error")
                        ? response.get("error").getAsString() : "Unknown error";
                        System.out.println("Login failed: " + error);
                    }
                }
            );

        }
        catch (Exception e)
        {
            System.out.println("Could not send login request: " + e.getMessage());
        }
        //System.out.println("Login attempt for user: " + username);
        //System.out.println("Login attempt sent. Awaiting server response.");
    }
    private void createAccount()
    {
        System.out.println("== Create Account ==");
        System.out.print("Email: ");
        String email = scn.next();
        System.out.print("Username: ");
        String username = scn.next();
        System.out.print("Password: ");
        String password = scn.next();
    
        System.out.print("Confirm Password: ");
        String confirmPassword = scn.next();
        if (!password.equals(confirmPassword))
        {
            System.out.println("Passwords do not match");
            return;
        }
        System.out.print("Pin between 4-8 numbers: ");
        String pin = scn.next();

        try
        {
            RequestManager.requestAccountCreation(email, username, password, pin, response ->
                {
                    boolean success = response.get("success").getAsBoolean();

                    if (success)
                    {
                        System.out.println("Account creation successful");
                    }
                    else
                    {
                        String error = response.has("error")
                        ? response.get("error").getAsString() : "Unknown error";
                        System.out.println("Account creation failed: " + error);
                    }
                }
            );
            //System.out.println("Account creation attempt for user: " + username + " with email: " + email);
            //System.out.println("Account creation attempt sent. Awaiting server response.");
        }
        catch (IllegalArgumentException e)
        {
            System.out.println("Invalid account information: " + e.getMessage());
            return;
        }    
        catch (Exception e)
        {
            System.out.println("Could not send account creation request: " + e.getMessage());
        }
    }
    private void recoverPassword()
    {
        System.out.println("== Recover Password ==");
        System.out.print("Enter your username: ");
        String username = scn.next();
        System.out.print("Enter your email: ");
        String email = scn.next();
        System.out.print("Enter your pin: ");
        String pin = scn.next();
        System.out.print("Enter your new password: ");
        String newPassword = scn.next();

        try
        {
            RequestManager.requestPasswordRecovery(username, email, pin, newPassword, response ->
                {
                    boolean success = response.get("success").getAsBoolean();

                    if (success)
                    {
                        System.out.println(response.get("message").getAsString());
                    }
                    else
                    {
                        String error = response.has("message")
                        ? response.get("message").getAsString() : "Unknown error";
                        System.out.println("Password recovery failed: " + error);
                    }
                }
            );
        }
        catch (Exception e)
        {
            System.out.println("Could not send password recovery request: " + e.getMessage());
        }
    }
}

