package com.green;

import javafx.application.Application;
import java.io.PrintWriter;
import java.net.ConnectException;
import java.net.Socket;
import java.util.Scanner;

public class Main {
    static String message = "";
    static String bridgeIp = "192.168.4.200"; 
    static int port = 5000;
    static boolean GUI = true;
    static Socket socket;
    static PrintWriter out;


    public static void main(String[] args) {
        System.out.println("connecting to " + bridgeIp + ":" + port);



        if(GUI){

            Thread GUIThread = new Thread(() -> {
                Application.launch(App.class, args);
            });
            GUIThread.start();
        }else{
            
        }
        try{
            Scanner scan = new Scanner(System.in);
            socket = new Socket(bridgeIp, port);
            out = new PrintWriter(socket.getOutputStream(), true);
            Thread inboxThread = new Thread(new Reader(socket));
            inboxThread.start();
            out.println("Hello from Ryan's laptop"); 
            while(!message.equals("end")){
                message = scan.nextLine();
                out.println(message);
            }
            scan.close();
            out.close();
            socket.close();
        }catch(ConnectException e){
            System.out.println("Connection stopped");
        }catch(Exception e){
            e.printStackTrace();
        }
    }
}