package com.green;

import java.util.concurrent.LinkedBlockingQueue;

public class MessageBridge {
    private static final LinkedBlockingQueue<String> queue = new LinkedBlockingQueue<>();

    public static LinkedBlockingQueue<String> getQueue() {
        return queue;
    }
}
