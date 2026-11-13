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

        String message = MessageFormatter.login(username, password);
        MessageBridge.getOutQueue().add(message);

        System.out.println("Login attempt for user: " + username);
        System.out.println("Login attempt sent. Awaiting server response.");
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
        //password requirement check client or server side?
        System.out.print("Confirm Password: ");
        String confirmPassword = scn.next();
        if (!password.equals(confirmPassword))
        {
            System.out.println("Passwords do not match");
            return;
        }

        try
        {
            String message = MessageFormatter.createAccount(email, username, password);
            MessageBridge.getOutQueue().add(message);
            System.out.println("Account creation attempt for user: " + username + " with email: " + email);
            System.out.println("Account creation attempt sent. Awaiting server response.");
        }
        catch (IllegalArgumentException e)
        {
            System.out.println(e.getMessage());
            return;
        }        
    }
}

