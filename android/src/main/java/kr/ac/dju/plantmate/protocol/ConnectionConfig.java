package kr.ac.dju.plantmate.protocol;

public class ConnectionConfig {

    private final ProtocolType protocolType;
    private final String host;
    private final int port;
    private final String clientId;

    public ConnectionConfig(ProtocolType protocolType, String host, int port, String clientId) {
        this.protocolType = protocolType;
        this.host = host;
        this.port = port;
        this.clientId = clientId;
    }

    public ProtocolType getProtocolType() {
        return protocolType;
    }

    public String getHost() {
        return host;
    }

    public int getPort() {
        return port;
    }

    public String getClientId() {
        return clientId;
    }
}
