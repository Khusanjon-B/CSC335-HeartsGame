package com.green;

import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.function.Consumer;

import com.google.gson.JsonObject;
import com.google.gson.JsonParser;

public class RequestManager
{
    private static final ConcurrentHashMap<Integer, Consumer<JsonObject>>
    pendingRequests = new ConcurrentHashMap<>();

    private static final AtomicInteger nextRequestId = new AtomicInteger(1);

    private static void addPendingRequest(int requestId, Consumer<JsonObject> callback)
    {
        pendingRequests.put(requestId, callback);
    }

    public static void handleResponse(int requestId, JsonObject response)
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

    public static void failPendingRequests(String message)
    {
        for (Consumer<JsonObject> callback : pendingRequests.values())
        {
            JsonObject response = new JsonObject();
            response.addProperty("success", false);
            response.addProperty("error", "CONNECTION_LOST");
            response.addProperty("message", message);
            callback.accept(response);
        }
        pendingRequests.clear();
    }

    public static void sendRequest(String message, Consumer<JsonObject> callback)
    {
        JsonObject json = JsonParser.parseString(message).getAsJsonObject();

        int requestId = nextRequestId.getAndIncrement();
        json.addProperty("requestId", requestId);
        addPendingRequest(requestId, callback);

        MessageBridge.getOutQueue().offer(json.toString());

    }

    public static void requestLogin(String username, String password, Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.login(username, password), callback);
    }

    public static void requestAccountCreation(String email, String username, String password,String pin, Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.createAccount(email, username, password, pin), callback);
    }

    public static void requestPasswordRecovery(String username, String email, String pin, String newPassword, Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.recoverPassword(username, email, pin, newPassword), callback);
    }

    public static void requestLogout(Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.logout(), callback);
    }

    public static void requestStartLobby(Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.startLobby(), callback);
    }

    public static void requestGetLobbies(Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.getLobbies(), callback);
    }

    public static void requestJoinLobby(int lobbyId, Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.joinLobby(lobbyId), callback);
    }

    public static void requestLeaveLobby(Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.leaveLobby(), callback);
    }

    public static void requestStartGame(Consumer<JsonObject> callback)
    {
        sendRequest(MessageFormatter.startGame(), callback);
    }
}
