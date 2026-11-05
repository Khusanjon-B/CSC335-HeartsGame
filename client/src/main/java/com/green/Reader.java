package com.green;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.Socket;

public class Reader implements Runnable{
    private Socket socket;
    private BufferedReader in;
    public Reader(Socket socket){
        this.socket = socket;
    }
    @Override
    public void run(){
        String response = "";
        try{
            in = new BufferedReader(new InputStreamReader(socket.getInputStream()));
            while(socket != null && !socket.isClosed() && (response = in.readLine()) != null){
                System.out.println("Server says: " + response);
            }
        }catch(Exception e){
            System.out.println("input stream connection ended");
        }
    }

}
