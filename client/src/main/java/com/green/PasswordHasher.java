package com.green;

import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HexFormat;
import java.security.NoSuchAlgorithmException;
public class PasswordHasher 
{
    public static String hashPassword(String username, String password) 
    {
        try 
        {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            String saltedPassword = username + ":" + password;
            byte[] hash = md.digest(saltedPassword.getBytes(StandardCharsets.UTF_8));

            
            return HexFormat.of().formatHex(hash);
            
        } 
        catch (NoSuchAlgorithmException e) 
        {
            throw new IllegalStateException("SHA-256 algorithm not available", e);
        }

    }
}
