package kr.ac.dju.plantmate.protocol.mqtt;

public class BrokerConfig {

    private final String host;
    private final int port;
    private final String clientId;

    public BrokerConfig(String host, int port, String clientId) {
        this.host = host;
        this.port = port;
        this.clientId = clientId;
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

    public String getServerUri() {
        return "tcp://" + host + ":" + port;
    }
}
