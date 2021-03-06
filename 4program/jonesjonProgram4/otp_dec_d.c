#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

//Prototypes
void error(const char*);
void fillAddrStruct(struct sockaddr_in*, int*, char*[]);
void setSocket(int*, struct sockaddr_in*);
void acceptConnection(socklen_t*, struct sockaddr_in*, int*, int*);
void getClientMessage(int*);
void successMessage(int*, int*);
void encryptMessage(char[], char[], int*);
int modulus(int,int);

int main(int argc, char* argv[])
{
    //Initialize necessary variables
    int listenSocketFD, establishedConnectionFD, portNumber, spawnPid, childExitMethod;
    FILE *inputFD, *outputFD, *keyFD;
    socklen_t sizeOfClientInfo;
    struct sockaddr_in serverAddress, clientAddress;

    //Check usage
    if(argc < 2)
    {
        fprintf(stderr, "USAGE: %s port\n", argv[0]);
        exit(0);
    }
    else
    {
        //Fill server address struct
        fillAddrStruct(&serverAddress, &portNumber, argv);

        //Set up socket for listening from
        setSocket(&listenSocketFD, &serverAddress);

        //Infinite loop so server can keep getting connections
        while(1)
        {
            //Accept a connection, blocking if one is not available until one connects
            acceptConnection(&sizeOfClientInfo, &clientAddress, &listenSocketFD, &establishedConnectionFD);
            
            //When a connection is made, spawn a new process to handle communication
            spawnPid = fork();
            if(spawnPid == -1)
            {
                fprintf(stderr, "BAD PROCESS\n");
            }
            else if(spawnPid == 0)
            {
                //Get message from client and display it
                getClientMessage(&establishedConnectionFD);
        
                //Close existing socket which is connected to the client
                close(establishedConnectionFD);
                exit(0);
            }
            //Parent
            else
            {
                //Wait for child to terminate and close established conneciton file descriptor
                waitpid(spawnPid, &childExitMethod, 0);
                close(establishedConnectionFD);
            }
        }    
        //Close the listening socket
        close(listenSocketFD);

    }

    return 0;
}

/*************************************************
 * Function: error
 * Description: prints an error to stderr and exits with exit code 1
 * Params: esit message
 * Returns: none
 * Pre-conditions: valid message recieved
 * Post-conditions: exits with value 1 and print message to stderr
 * **********************************************/
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/*************************************************
 * Function: fillAddrStruct
 * Description: Sets up the server address struct and fills other information like the port number
 * Params: address of sockaddr_in struct, address of portnumber integer, command line args
 * Returns: none
 * Pre-conditions: proper addresses and arguments are passed in
 * Post-conditions: server address struct is filled and port number is given. Exit if errors.
 * **********************************************/
void fillAddrStruct(struct sockaddr_in* serverAddress, int* portNumber, char* argv[])
{
    //Clear out address struct and obtain port number from command line
    memset((char*)serverAddress, '\0', sizeof(serverAddress));
    *portNumber = atoi(argv[1]);

    //Create network capable socket
    serverAddress->sin_family = AF_INET;
    //Store port number and convert from LSB to MSB form
    serverAddress->sin_port = htons(*portNumber);
    //Allow any address for connection
    serverAddress->sin_addr.s_addr = INADDR_ANY;
}

/*************************************************
 * Function: setSocket
 * Description: sets up socket for communication with client
 * Params: address of listening socket file descriptor var, address of server address struct
 * Returns: none
 * Pre-conditions: correct arguments passed in
 * Post-conditions: listening socket file descriptor is set and exits on error
 * **********************************************/
void setSocket(int* listenSocketFD, struct sockaddr_in* serverAddress)
{
    //Fill listening file descriptor
    *listenSocketFD = socket(AF_INET, SOCK_STREAM, 0);
    if(*listenSocketFD < 0)
    {
        error("otp_dec_d error: opening socket");
    }
    if(bind(*listenSocketFD, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) < 0)
    {
        error("otp_dec_d error: on binding");
    }
    
    //Listen for a connection, up to 5
    listen(*listenSocketFD, 5);
}

/*************************************************
 * Function: acceptConnection
 * Description: Checks to see if the client is actually the correct client trying to connect by communicating identification bits to it
 * Params: address of struct that holds size of client info, address for clientaddress struct, 
 * address to listening file descriptor address to established connection file descriptor
 * Returns: none
 * Pre-conditions: Server has a listening socket
 * Post-conditions: Valid connection has been made and the file descriptors and structs passed in have been changed accordingly
 * **********************************************/
void acceptConnection(socklen_t* sizeOfClientInfo, struct sockaddr_in* clientAddress, int* listenSocketFD, int* establishedConnectionFD)
{
    int charsWritten, charsRead;
    char buffer[1];
    memset(buffer, '\0', 1);

    //Infinite loop until a valid connection has been made
    while(1)
    {
        //Get size of client indo
        *sizeOfClientInfo = sizeof(*clientAddress);
        //Accept a connection and fill the established connection file descriptor
        *establishedConnectionFD = accept(*listenSocketFD, (struct sockaddr *)clientAddress, sizeOfClientInfo);
        //Check for basic errors on accept
        if(*establishedConnectionFD < 0)
        {
            close(*establishedConnectionFD);
            fprintf(stderr, "otp_dec_d error: on accept\n");
        }
        //If no errors
        else
        {
            //Recieve message of indicator bit from client
            charsRead = recv(*establishedConnectionFD, buffer, sizeof(buffer), 0);
            if(charsRead < 0)
            {
                close(*establishedConnectionFD);
                fprintf(stderr, "otp_dec_d error: recieving identifier bit from client\n");
            }
            else if(charsRead == 0)
            {
                close(*establishedConnectionFD);
                fprintf(stderr, "otp_dec_d error: charsRead 0 when recieving identifier bit from client");
            }
            //If no errors
            else
            {
                //Send the server indicator bit to the client
                charsWritten = send(*establishedConnectionFD, "1", 1, 0);
                //Check for basic send errors
                if(charsWritten < 0)
                {
                    close(*establishedConnectionFD);
                    fprintf(stderr, "otp_dec_d error: sending identifier bit to client\n");
                }
                else if(charsWritten == 0)
                {
                    close(*establishedConnectionFD);
                    fprintf(stderr, "otp_dec_d error: charsWritten 0 when sending identifier bit to client\n");
                }
        
                //Check identifier bit recieved
                if(buffer[0] == '1')
                {
                    //If a good bit was recieved, get out of the infinite loop
                    break;
                }
            }
        }
     } 

}

/*************************************************
 * Function: getClientMessage
 * Description: Gets the plaintext file string and the key string from the client and puts it into buffers then calls an encrypt message function
 * Params: address of established connection file descriptor
 * Returns: none
 * Pre-conditions: established connection file descriptor is open and valid
 * Post-conditions: plaintext file string and key file string have been stored into buffers and put into the encryption function
 * **********************************************/
void getClientMessage(int* establishedConnectionFD)
{
    //Initialize buffers
    char fileMessage[80000], keyMessage[80000], readBuffer[2];
    int charsRead;

    //Get the plaintext string
    memset(fileMessage, '\0', 80000);
    //While the control character '0' is not found, keep calling recv and filling the buffer
    while(strstr(fileMessage, "0") == NULL)
    {
        memset(readBuffer, '\0', sizeof(readBuffer));
        charsRead = recv(*establishedConnectionFD, readBuffer, sizeof(readBuffer)-1, 0);
        strcat(fileMessage, readBuffer);
        if (charsRead == -1) break;
        if (charsRead == 0) break;
    }
    //Get the key string
    memset(keyMessage, '\0', 80000);
    //While the control character '0' is not found, keep calling recv and filling the buffer
    while(strstr(keyMessage, "0") == NULL)
    {
        memset(readBuffer, '\0', sizeof(readBuffer));
        charsRead = recv(*establishedConnectionFD, readBuffer, sizeof(readBuffer)-1, 0);
        strcat(keyMessage, readBuffer);
        if (charsRead == -1) break;
        if (charsRead == 0) break;
    }

    //Remove 0 from the strings
    fileMessage[strcspn(fileMessage, "0")] = '\0';
    keyMessage[strcspn(keyMessage, "0")] = '\0';
    encryptMessage(fileMessage, keyMessage, establishedConnectionFD);

}

/*************************************************
 * Function: encryptMessage
 * Description: Uses the key to do a one time pad type encryption on the plaintext file received
 * Params: string file message, string key message, address of established connection file descriptor
 * Returns: none
 * Pre-conditions: file message contains data, key message contains data, established connection file descriptor is open and valid
 * Post-conditions: message is encrypted and sent to the client
 * **********************************************/
void encryptMessage(char fileMessage[], char keyMessage[], int* establishedConnectionFD)
{
    int charsRead, i, pos, num, a, b;
    //Cipher text buffer is the same size as the file message
    char cipherText[strlen(fileMessage)];
    //Alphabet for quick access
    char alphabetASCII[27] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

    //Clean cipher text
    memset(cipherText, '\0', sizeof(cipherText));
    //For each character in the file message
    for(i=0;i<strlen(fileMessage);i++)
    {
        //Is the character not whitespace
        if(fileMessage[i] != 32)
        {
            //Subtract ASCII for A from filemessage character for position in alphabet
            a = fileMessage[i]-'A';
        }
        //If whitespace
        else
        {
            //Position in alphabet string for whitespace
            a = 26;
        }
        //Is the character not whitespace
        if ((keyMessage[i]) != 32)
        {
            //Subtract ASCII for A from keymessage character for position in alphabet
            b = keyMessage[i]-'A';
        }
        //If whitespace
        else
        {
            //position in alphabet string for whitespace
            b = 26;
        }

        //Num for modulus operation
        num = a-b;
        //Set cipher text at position i equal to the modulus result of num and 27
        cipherText[i] = alphabetASCII[modulus(num,27)];
    }

    //Find null terminator in ciphertext and add control character and another null terminator
    pos = strcspn(cipherText, "\0");
    strcpy(&cipherText[pos], "0\0");

    //Send cipher text
    charsRead = send(*establishedConnectionFD, cipherText, sizeof(cipherText), 0);
    if(charsRead < 0)
    {
        error("otp_dec_d error: writing to socket");
    }

}

/*************************************************
 * Function: modulus
 * Description: Handles negative numbers in the modulus operation that % will not handle
 * Params: Two integers to be operated on
 * Returns: integer result
 * Pre-conditions: none
 * Post-conditions: returns correct operation result
 * **********************************************/
int modulus(int num1, int num2)
{
    int result;
    result = num1 % num2;
    //If number is negativem add num2 to num1
    if(result < 0)
    {
        result += num2;
    }
    return result;
}
