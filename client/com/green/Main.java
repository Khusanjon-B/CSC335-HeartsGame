package com.green;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        //10.100.63.182
        String message = "";
        String bridgeIp = "192.168.4.200"; 
        int port = 5000;

        System.out.println("connecting to " + bridgeIp + ":" + port);

        try{
            Scanner scan = new Scanner(System.in);
            Socket socket = new Socket(bridgeIp, port);
            PrintWriter out = new PrintWriter(socket.getOutputStream(), true);
            Thread inboxThread = new Thread(new Reader(socket));
            inboxThread.start();
            out.println("Hello from Ryan's laptop"); 
            while(!message.equals("end")){
                message = scan.nextLine();
                out.println(message);
            }
            scan.close();
            socket.close();
        }catch(Exception e){
            e.printStackTrace();
        }
    }
}