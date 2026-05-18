package kr.ac.dju.plantmate.model;

import kr.ac.dju.plantmate.protocol.ProtocolType;

public class SessionState {

    private final boolean connected;
    private final boolean loggedIn;
    private final int userId;
    private final String loginId;
    private final ProtocolType protocolType;

    public SessionState(boolean connected, boolean loggedIn, int userId, String loginId, ProtocolType protocolType) {
        this.connected = connected;
        this.loggedIn = loggedIn;
        this.userId = userId;
        this.loginId = loginId;
        this.protocolType = protocolType;
    }

    public boolean isConnected() {
        return connected;
    }

    public boolean isLoggedIn() {
        return loggedIn;
    }

    public int getUserId() {
        return userId;
    }

    public String getLoginId() {
        return loginId;
    }

    public ProtocolType getProtocolType() {
        return protocolType;
    }
}
