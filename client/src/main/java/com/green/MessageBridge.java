package com.green;

import java.util.concurrent.LinkedBlockingQueue;

public class MessageBridge {
    private static final LinkedBlockingQueue<String> inQueue = new LinkedBlockingQueue<>();
    private static final LinkedBlockingQueue<String> outQueue = new LinkedBlockingQueue<>();

    public static LinkedBlockingQueue<String> getInQueue() {
        return inQueue;
    }

    public static LinkedBlockingQueue<String> getOutQueue() {
        return outQueue;
    }

}
