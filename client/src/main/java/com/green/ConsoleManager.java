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
            System.out.println("1. Login");
            System.out.println("2. Create Account");
            System.out.println("3. Exit");
            System.out.println("4. Recover Password");

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
                default:
                    System.out.println("Invalid choice, try again");
                    break;
            }
        }
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
            String message = MessageFormatter.login(username, password);
            RequestManager.sendRequest(message, response -> 
                {
                    boolean success = response.get("success").getAsBoolean();

                    if (success)
                    {
                        System.out.println("Login successful");
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
        int pin = scn.nextInt();

        try
        {
            String message = MessageFormatter.createAccount(email, username, password, pin);
            RequestManager.sendRequest(message, response -> 
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
        System.out.print("Enter your email or username: ");
        String input = scn.next();
        System.out.print("Enter your pin: ");
        int pin = scn.nextInt();

        try
        {
            String message = MessageFormatter.recoverPassword(input, pin);
            RequestManager.sendRequest(message, response -> 
                {
                    boolean success = response.get("success").getAsBoolean();

                    if (success)
                    {
                        System.out.println("Password recovery successful.");
                    }
                    else
                    {
                        String error = response.has("error")
                        ? response.get("error").getAsString() : "Unknown error";
                        System.out.println("Password recovery failed: " + error);
                    }
                }
            );
            //System.out.println("Password recovery attempt for input: " + input);
            //System.out.println("Password recovery attempt sent. Awaiting server response.");
        }
        catch (Exception e)
        {
            System.out.println("Could not send password recovery request: " + e.getMessage());
        }
    }
}

