package com.green;

import java.util.ArrayList;
import java.util.List;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;

public final class LobbyJson {
    private LobbyJson() {
    }

    public static List<String> playerNames(JsonArray players) {
        List<String> names = new ArrayList<>();
        for (JsonElement player : players) {
            if (player.isJsonPrimitive() && player.getAsJsonPrimitive().isString()) {
                names.add(player.getAsString());
            } else if (player.isJsonObject() && hasString(player.getAsJsonObject(), "username")) {
                names.add(player.getAsJsonObject().get("username").getAsString());
            } else {
                throw new IllegalArgumentException("Invalid lobby player entry.");
            }
        }
        return List.copyOf(names);
    }

    public static String hostName(JsonObject lobby) {
        if (hasString(lobby, "hostUsername")) {
            return lobby.get("hostUsername").getAsString();
        }
        if (hasString(lobby, "host")) {
            return lobby.get("host").getAsString();
        }
        if (lobby.has("hostId") && lobby.has("players")) {
            int hostId = lobby.get("hostId").getAsInt();
            for (JsonElement player : playerArray(lobby)) {
                if (player.isJsonObject()) {
                    JsonObject member = player.getAsJsonObject();
                    if (member.has("playerId") && member.get("playerId").getAsInt() == hostId
                            && hasString(member, "username")) {
                        return member.get("username").getAsString();
                    }
                }
            }
            throw new IllegalArgumentException("Lobby hostId does not match a player.");
        }
        JsonArray players = lobby.has("players") && lobby.get("players").isJsonArray()
            ? lobby.getAsJsonArray("players")
            : null;
        if (players != null && !players.isEmpty()) {
            return playerNames(players).get(0);
        }
        throw new IllegalArgumentException("Lobby response does not include its host.");
    }

    public static int playerNumber(JsonObject lobby, Player currentPlayer) {
        if (lobby.has("playerNum")) {
            return lobby.get("playerNum").getAsInt();
        }
        JsonArray players = playerArray(lobby);
        for (int index = 0; index < players.size(); index++) {
            JsonElement member = players.get(index);
            if (member.isJsonObject() && member.getAsJsonObject().has("playerId")
                    && member.getAsJsonObject().get("playerId").getAsInt() == currentPlayer.getPlayerId()) {
                return index + 1;
            }
            if (member.isJsonPrimitive() && member.getAsJsonPrimitive().isString()
                    && member.getAsString().equals(currentPlayer.getUsername())) {
                return index + 1;
            }
        }
        throw new IllegalArgumentException("Current player is missing from lobby response.");
    }

    private static JsonArray playerArray(JsonObject lobby) {
        if (!lobby.has("players") || !lobby.get("players").isJsonArray()) {
            throw new IllegalArgumentException("Lobby response does not include a player list.");
        }
        return lobby.getAsJsonArray("players");
    }

    private static boolean hasString(JsonObject object, String field) {
        if (!object.has(field) || !object.get(field).isJsonPrimitive()) {
            return false;
        }
        JsonPrimitive value = object.getAsJsonPrimitive(field);
        return value.isString();
    }
}
