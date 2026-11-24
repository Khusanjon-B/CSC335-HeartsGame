package com.green;



public class Player {
    private static volatile Player currentPlayer;

    private final int playerId;
    private final String username;
    private int lobbyId;
    private int playerNum;

    private Player(int playerId, String username) {
        this.playerId = playerId;
        this.username = username;
    }

    public static Player getCurrentPlayer() {
        return currentPlayer;
    }

    public static void setupPlayer(int id, String name) {
        currentPlayer = new Player(id, name);
    }

    public int getPlayerId() {
        return playerId;
    }

    public String getUsername() {
        return username;
    }

    public boolean isHost() {
        return (playerNum == 1);
    }

    public int getLobbyId() {
        return lobbyId;
    }

    public int getPlayerNum() {
        return playerNum;
    }

    public void setPlayerNum(int playerNum) {
        this.playerNum = playerNum;
    }

    public void joinGame(int playerNum, int lobbyId) {
        this.playerNum = playerNum;
        this.lobbyId = lobbyId;
    }

    public void leaveGame() {
        lobbyId = 0;
        playerNum = 0;
    }

    public static void resetPlayer() {
        currentPlayer = null;
    }
}
