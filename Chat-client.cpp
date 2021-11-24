#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지
#pragma comment(lib, "ws2_32")
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>


#define SERVERIP   "127.0.0.1"
#define MULTICASTIP "235.7.8.9"
#define SERVERPORT 9000
#define REMOTEPORT 9010
#define BUFSIZE    512

int quit_Thread = 1;


// 소켓 함수 오류 출력 후 종료
void err_quit(const char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(NULL, (LPCTSTR)lpMsgBuf, msg, MB_ICONERROR);
    LocalFree(lpMsgBuf);
    exit(1);
}

// 소켓 함수 오류 출력
void err_display(const char* msg)
{
    LPVOID lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, WSAGetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    printf("[%s] %s", msg, (char*)lpMsgBuf);
    LocalFree(lpMsgBuf);
}

DWORD WINAPI sendtoThread(LPVOID arg) {

    SOCKET sock = (SOCKET)arg;
    int retval, len;
    char buf[BUFSIZE + 1];

    // 전송 주소 ( 클라이언트 -> 서버 ) 
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVERPORT);

    while (quit_Thread) {

        if (fgets(buf, BUFSIZE, stdin) == NULL) break;

        if (quit_Thread == 0) break;

        len = strlen(buf);
        if (buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }
        retval = sendto(sock, buf, BUFSIZE, 0, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
        if (retval == SOCKET_ERROR) {
            err_display("sendto()");
            continue;
        }
    }
    return 0;
}

DWORD WINAPI recvfromThread(LPVOID arg) {

    SOCKET sock = (SOCKET)arg;
    int retval;
    char buf[BUFSIZE + 13];
    char* name;
    char* context;
    SOCKADDR_IN recvaddr;

    while (quit_Thread) {
        int addrlen = sizeof(recvaddr);
        retval = recvfrom(sock, buf, BUFSIZE+13, 0, (SOCKADDR*)&recvaddr, &addrlen);
        if (retval == SOCKET_ERROR) {
            err_display("recvfrom()");
            continue;
        }
        else if (retval > 0) {
            buf[retval - 1] = '\0';
            name = strtok_s(buf, "\n", &context);
            printf("[%s] %s\n", name,context);
            if (!strncmp(context,"fin",3)) {
                printf("연결종료\n");
                quit_Thread = 0;
                break;
            }
        }
        else if (retval == 0) {
            break;
        }

    }
    return 0;
}

void stdin_reset() {
    while (getchar() != '\n');
}


int main(int argc, char* argv[])
{
    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) err_quit("socket()");

    // 소켓 옵션 설정

    BOOL optval = TRUE;
    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
    if (retval == SOCKET_ERROR) err_quit("setsockopt()");


    // 클라이언트에서 멀티캐스트를 하기위한 지역 주소 바인딩 
    /*
    SOCKADDR_IN clientaddr;
    ZeroMemory(&clientaddr, sizeof(clientaddr));
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_port = htons(REMOTEPORT);
    clientaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    retval = bind(sock, (SOCKADDR*)&clientaddr, sizeof(clientaddr));
    if (retval == SOCKET_ERROR) err_quit("bind()");
    */
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_addr.s_addr = inet_addr(SERVERIP);
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(SERVERPORT);
    SOCKADDR_IN recvaddr;
    int addrlen =sizeof(recvaddr);

    // 통신에 필요한 스레드 초기화
    HANDLE SThread[2];
    int namelen;
    char buf[BUFSIZE+1];
    char name[12];

    // 서버와 연결 과정
    while (true) {
        // 닉네임 입력받기
        printf("[접속 설정] 사용하실 닉네임을 입력해주세요 (1~6자)\n");
        if (fgets(name, 11, stdin) == NULL) continue;
        
        //namelen = strlen(name);
        //name[namelen-1] = '\0';
        //stdin_reset();

        // 서버에 연결 요청
        strcpy_s(buf, "cnt");
        strcat_s(buf, name);
        printf("서버요청 패킷 %s\n", buf);

        retval = sendto(sock, buf, 16 , 0, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
        if (retval == SOCKET_ERROR) {
            err_display("sendto()");
            continue;
        }
        retval = recvfrom(sock, buf, BUFSIZE+1, 0, (SOCKADDR*)&recvaddr, &addrlen);
        if (retval == SOCKET_ERROR) {
            err_display("recvfrom()");
            continue;
        }
        if (!strcmp(buf, "cntack")) {
            break;
        }

        printf("서버 연결 실패 \n");
    }
    

    printf("***************[채팅 클라이언트 시작]***************\n");

    SThread[0] = CreateThread(NULL, 0, recvfromThread, (LPVOID)sock, 0, NULL);
    if (SThread[0] == NULL) err_display("CreateThread(recvThread())");

    SThread[1] = CreateThread(NULL, 0, sendtoThread, (LPVOID)sock, 0, NULL);
    if (SThread[1] == NULL) err_display("CreateThread(sendThread())");


    // 데이터 송신, 수신 스레드가 모두 종료했을 때
    WaitForMultipleObjects(2, SThread, TRUE, INFINITE);


    closesocket(sock);
    printf("***************[채팅 클라이언트 종료]***************\n");
    // 윈속 종료
    WSACleanup();
    return 0;
}
