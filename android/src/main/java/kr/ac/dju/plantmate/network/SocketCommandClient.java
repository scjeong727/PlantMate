package kr.ac.dju.plantmate.network;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;

public class SocketCommandClient {

    private Socket socket;
    private InputStream inputStream;
    private OutputStream outputStream;

    public boolean isConnected() {
        return socket != null && socket.isConnected() && !socket.isClosed();
    }

    public synchronized void connect(String host, int port) throws IOException {
        close();

        Socket newSocket = new Socket();
        newSocket.connect(new InetSocketAddress(host, port), 5000);
        newSocket.setSoTimeout(5000);

        socket = newSocket;
        inputStream = newSocket.getInputStream();
        outputStream = newSocket.getOutputStream();
    }

    public synchronized String sendCommand(String command) throws IOException {
        if (!isConnected()) {
            throw new IOException("서버 연결이 없습니다.");
        }

        byte[] request = (command + "\n").getBytes(StandardCharsets.UTF_8);
        outputStream.write(request);
        outputStream.flush();

        byte[] buffer = new byte[4096];
        int len = inputStream.read(buffer);
        if (len <= 0) {
            throw new IOException("서버 응답이 없습니다.");
        }

        return new String(buffer, 0, len, StandardCharsets.UTF_8).trim();
    }

    public synchronized void close() throws IOException {
        IOException firstError = null;

        if (inputStream != null) {
            try {
                inputStream.close();
            } catch (IOException e) {
                firstError = e;
            }
            inputStream = null;
        }

        if (outputStream != null) {
            try {
                outputStream.close();
            } catch (IOException e) {
                if (firstError == null) {
                    firstError = e;
                }
            }
            outputStream = null;
        }

        if (socket != null) {
            try {
                socket.close();
            } catch (IOException e) {
                if (firstError == null) {
                    firstError = e;
                }
            }
            socket = null;
        }

        if (firstError != null) {
            throw firstError;
        }
    }
}
