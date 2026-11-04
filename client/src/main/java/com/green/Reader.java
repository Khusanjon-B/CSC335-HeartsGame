package com.green;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.Socket;
import java.net.SocketException;

public class Reader implements Runnable{
    private Socket socket;
    private BufferedReader in;
    private String response = "";
    public Reader(Socket socket){
        this.socket = socket;
        try{
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
        }catch(Exception e){
            e.printStackTrace();
        }
    }
    @Override
    public void run(){
        try{
            while(socket != null && !socket.isClosed() && (response = in.readLine()) != null){
                System.out.println("ESP32 says: " + response);
            }
        }catch(SocketException e){
            System.out.println("connection ended");
        }catch(Exception e){
            e.printStackTrace();
        }
    }

}
