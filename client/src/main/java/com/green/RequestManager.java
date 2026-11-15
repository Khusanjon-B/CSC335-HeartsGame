package com.green;

import java.util.UUID;
import java.util.concurrent.ConcurrentHashMap;
import java.util.function.Consumer;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

public class RequestManager 
{
    private static final ConcurrentHashMap<String, Consumer<JsonObject>> 
    pendingRequests = new ConcurrentHashMap<>();

    private static void addPendingRequest(String requestId, Consumer<JsonObject> callback)
    {
        pendingRequests.put(requestId, callback);
    }

    public static void handleResponse(String requestId, JsonObject response)
    {
        Consumer<JsonObject> callback = pendingRequests.remove(requestId);
        if (callback != null)
        {
            callback.accept(response);
        }
        else
        {
            System.out.println("No pending request found for requestId: " + requestId);
        }
    }
    public static void sendRequest(String message, Consumer<JsonObject> callback)
    {
        JsonObject json = JsonParser.parseString(message).getAsJsonObject();

        String requestId = UUID.randomUUID().toString();
        json.addProperty("requestId", requestId);
        addPendingRequest(requestId, callback);

        MessageBridge.getOutQueue().offer(json.toString());

    }
}
