#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdbool.h>
#include <curl/curl.h>
#include <time.h>
#include <pthread.h>
#include <signal.h> 
#include <openssl/sha.h> 
#include <openssl/buffer.h> 


#define UPDATE_DRAWING_JSON_TIME 5 //define the interval of update Drawing_Json(JsonStr and scriptTag)
#define TIMES_UPDATE_BEFORE_SAVE 3 //define the interval of saving datacache to file(UPDATE_DRAWING_JSON_TIME*TIMES_UPDATE_BEFORE_SAVE)(s)
#define TIME_TO_CLEANUP 60 //define the interval of checking which thread to clean up
#define DRAWING_FILE "drawing.txt"
#define LOG_FILE "log.txt"
#define DRAWING_TIME_FILE "drawingTime.txt"
#define MAX_EVENTS 10 //define the max number of events epoll_wait can handle
#define PORT 8080
#define ARRAY_SIZE 100 //define the length of drawing
#define MAX_VALUE 63 //define the range of colour code,which means 64 different colours
#define MAX_CLIENTS 100  // the max number of clients active



typedef struct UpdateTransaction {
    int row;
    int col;
    int newValue;
    time_t timestamp; // TimeStamp
} UpdateTransaction;

typedef struct TransactionNode {
    UpdateTransaction transaction;
    struct TransactionNode* next;
} TransactionNode;

typedef enum {
    WS_STATE_HTTP,    // Initial HTTP status
    WS_STATE_HANDSHAKE, // Handshake in progress
    WS_STATE_CONNECTED // WebSocket connection established
} ws_state_t;

typedef struct {
    int fd;             
    ws_state_t state;   
    time_t lastActivity;  
} ws_client_t;


// Global variables for transaction lists and thread control
TransactionNode *transactionList1 = NULL;
TransactionNode *transactionList2 = NULL;
TransactionNode **currentProcessingList = &transactionList1; // Pointer to current processing list
TransactionNode **currentListeningList = &transactionList2;  // Pointer to current listening list

pthread_mutex_t transactionMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex for synchronizing access
pthread_cond_t canProcess = PTHREAD_COND_INITIALIZER;         // Condition variable for signaling

ws_client_t clients[MAX_CLIENTS]; // the array to save the client
int client_count = 0;             // the number of client
int savedTime = 0;
int updateJsonTime = 0;
int initFlag = 0;
int processFlag = 0; // Flag to control which thread is processing transactions
int jsonInitFlag = 0;
int updateFlag = 0;  // mark the thread can update jsonStr
int interval;//test user add transactions' interval(s)
int processTime=0;
int addTransactionTime=0;
int endFlag = 0;
volatile sig_atomic_t shutdownFlag = 0; // Global flag to control shutdown
time_t nextUpdateTime;
char *jsonStr; 
char *gamehtml; 
char *htmlContent;
char *scriptTag;
char *http404Response = "HTTP/1.1 404 Not Found\r\n"
                             "Content-Length: 0\r\n"  // Assuming no body content
                             "Content-Type: text/html\r\n"  // Content type can be text/html or others
                             "Connection: close\r\n\r\n"; // Header ends with a blank line
// Global array for storing drawing and drawing time
int drawing[ARRAY_SIZE][ARRAY_SIZE];
time_t drawingTime[ARRAY_SIZE][ARRAY_SIZE];
pthread_mutex_t lock;
pthread_cond_t canUpdate;
pthread_mutex_t client_mutex;

void* cleanup_task(void* arg);
void parse_json(const char* const body, int* row, int* col, int* newValue); 
void updateHtmlcontent();
void loadHtml();
void initGlobals();
void updateDrawingToJson();
void addTransaction(TransactionNode **list, int row, int col, int newValue,int userId);
void processTransactions(TransactionNode **list);
void *threadWork(void *arg);
void* transactionGenerator(void* arg);
void updateDrawingTime(const char *filename, int row, int col, time_t newTime); 
void initializeAndWriteDrawingTime(const char *filename, time_t initialTime);
void readDrawingTimeFromFile(const char *filename);
void writeDrawingTimeToFile(const char *filename);
void updateArray(const char *filename, int row, int col, int newValue);
void initializeAndWriteArray(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]);
void readArrayFromFile(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]);
void writeArrayToFile(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]);
void run_server(int sfd);
int initialize_server(int port);
void setup_epoll(int sfd, int *efd);
void handle_new_connection(int sfd, int efd);
bool parse_request(const char *request, char *method, char *filename);
void handle_post_request(int client_fd, char* body); 
void send_response(int client_fd, const char *method, const char *filename); 
void log_error(const char *msg);
void send_error_response(int client_fd, const char *status, const char *message);
void handle_client_request(int client_fd);
void add_client(int client_fd);
void remove_client(int client_fd);
void update_last_activity(int client_fd);
int make_socket_non_blocking(int sfd);
void handle_websocket_handshake(int client_fd, const char *buf);
void broadcast_to_clients(int row, int col, int newValue);
void send_websocket_message(int client_fd, int row, int col, int newValue);
void handle_sigint(int sig);
void gennerateTestUsers(int testNum,int interval,int sleepTime);


//Email service
// Function to generate a six-digit random number as a verification code
//void generateVerificationCode(char *code){
//    int i;
//    int randomNum;
//
//    srand(time(NULL)); // Seed the random number generator
//
//    for (i = 0; i < VERIFYCODE_LENGTH; ++i){
//        randomNum = rand() % 10; // Generate a random number between 0 and 9
//        code[i] = '0' + randomNum; // Convert the number to a character and store it
//    }
//
//    code[VERIFYCODE_LENGTH] = '\0'; // Null-terminate the string
//}
//
//
//// Function to create a message with the verification code
//void createVerificationMessage(const char *code, char *message){
//    sprintf(message,
//            "Subject: Your Verification Code for Let's Paint Together\n\n"
//            "Hello!\n\n"
//            "Thank you for playing \"Let's Paint Together\". Your verification code is: %s.\n\n"
//            "Please enter this code to continue with your login or register process. This code ensures a secure access to your account.\n\n"
//            "Happy painting!\n\n"
//            "Best,\n"
//            "The Let's Paint Together Team",
//            code
//           );
//}
//
////startEmail Service
////export EMAIL_USERNAME="your-username"
////export EMAIL_PASSWORD="your-password"
//
//void initialEmailService(){
//    username = getenv("EMAIL_USERNAME");
//    password = getenv("EMAIL_PASSWORD");
//    //You should set the environment variables
//    if (username == NULL || password == NULL){
//        fprintf(stderr, "\nEnvironment variables 'EMAIL_USERNAME' and 'EMAIL_PASSWORD' are not set. "
//                "Please set these variables to enable authentication. "
//                "Without these, the login verification feature (e.g., CAPTCHA) will be disabled, "
//                "allowing direct login without a verification code.\n");
//                bool emailService = false;
//    }
//    else{
//        emailService = true;
//        printf("\nEmail service initialized successfully with the provided credentials.\n");
//    }
//    return;
//}
//
////send Email(content code)
//int sendEmaiCodel(User *user){
//    CURL *curl;
//    CURLcode res = CURLE_OK;
//
//    curl = curl_easy_init();
//    if(user==NULL){
//        printf("Can't get the userInfomation.");
//        return;
//    }
//
//    char *messageToSend;
//    createVerificationMessage(user->code,messageToSend);
//
//    if(curl){
//        // Set the URL of SMTP client
//        curl_easy_setopt(curl, CURLOPT_URL, "smtp://smtp.163.com:587");
//
//        // start STARTTLS
//        curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
//
//
//        curl_easy_setopt(curl, CURLOPT_USERNAME, username);
//        curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
//
//
//        // SMTP verify
//        curl_easy_setopt(curl, CURLOPT_USERNAME, username);
//        curl_easy_setopt(curl, CURLOPT_PASSWORD, password);
//
//        // sender
//        struct curl_slist *recipients = NULL;
//        curl_easy_setopt(curl, CURLOPT_MAIL_FROM, username);
//
//        // accepter
//        recipients = curl_slist_append(recipients, user->email);
//        curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
//
//        // the email content
//        curl_easy_setopt(curl, CURLOPT_READDATA, messageToSend);
//        curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
//
//        // send email
//        res = curl_easy_perform(curl);
//
//        // check the res
//        if(res != CURLE_OK)
//            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
//
//        // clear
//        curl_slist_free_all(recipients);
//        curl_easy_cleanup(curl);
//    }
//
//    return (int)res;
//}
//
////Model-user
//
//// Function to create and add a new user to the global list
//// newData: Pointer to the new user data
//void createUser(User *newData){
//    User *newUser = malloc(sizeof(User));
//    if (newUser == NULL){
//        perror("Failed to allocate memory for new user");
//        return;
//    }
//
//    // Copy new user data
//    strcpy(newUser->ID, newData->ID);
//    strcpy(newUser->email, newData->email);
//    strcpy(newUser->pwd, newData->pwd);
//    strcpy(newUser->code, newData->code);
//    strcpy(newUser->name, newData->name);
//
//    // Initialize other fields
//    //set status 1
//    newUser->status = 1;
//    newUser->blockCount = 0;
//    newUser->lastDrawTime = 0; // Set to current time
//    newUser->lastGetCodeTime = 0;
//    newUser->next = NULL;
//
//    // Add the new user to the global list
//    if (users == NULL){
//        users = newUser; // If list is empty, set new user as the head
//    }
//    else{
//        User *current = users;
//        while (current->next != NULL)
//        {
//            current = current->next;
//        }
//        current->next = newUser; // Append new user at the end of the list
//    }
//
//    // Write updated list to file
//    writeUsersToFile("user.txt", users);
//}
//
//
//// update the info of user
////updateUser(users, "123456", &updatedData);
//void updateUser(User *users, const char *ID, const User *newData){
//    User *current = users;
//    while (current){
//        if (strcmp(current->ID, ID) == 0){
//            strcpy(current->email, newData->email);
//            strcpy(current->pwd, newData->pwd);
//            strcpy(current->code, newData->code);
//            strcpy(current->name, newData->name);
//            current->blockCount = newData->blockCount;
//            current->lastDrawTime = newData->lastDrawTime;
//            current->lastGetCodeTime = newData->lastGetCodeTime;
//            break;
//        }
//        current = current->next;
//    }
//    writeUsersToFile(USER_FILE,users);
//}
//
//// Release user linked list memory
//void freeUserList(User *users){
//    User *current = users;
//    while (current){
//        User *temp = current;
//        current = current->next;
//        free(temp);
//    }
//}
//
//User* readUsersFromFile(const char *filename) {
//    printf("Attempting to open file: %s\n", filename);
//    FILE *file = fopen(filename, "r");
//    if (!file) {
//        file = fopen(filename, "w+");
//        if (!file) {
//            perror("Error opening file");
//            return NULL;
//        } else {
//            printf("File created successfully as it didn't exist: %s\n", filename);
//        }
//    } else {
//        printf("File opened successfully: %s\n", filename);
//    }
//
//    User *head = NULL, *tail = NULL;
//    int count = 0;
//
//    while (1) {
//        User *newUser = malloc(sizeof(User));
//        if (newUser == NULL) {
//            perror("Failed to allocate memory for new user");
//            break;
//        }
//
//        // Example fscanf format - adjust according to your User structure
//        int result = fscanf(file, "%49s\n", newUser->ID); // Adjust according to actual format and fields
//
//        if (result == 1) { // Adjust the number according to the number of inputs you expect
//            newUser->next = NULL;
//            if (head == NULL) {
//                head = newUser;
//                tail = newUser;
//            } else {
//                tail->next = newUser;
//                tail = newUser;
//            }
//            count++;
//        } else {
//            free(newUser);
//            if (result == EOF) {
//                printf("End of file reached. Total users read: %d\n", count);
//                break;
//            } else {
//                // Handle partial read or format error
//                perror("Error reading file");
//                break;
//            }
//        }
//    }
//
//    fclose(file);
//    printf("Finished reading file. Total users read: %d\n", count);
//    return head;
//}
//
//// write userData
//void writeUsersToFile(const char *filename, User *users){
//    FILE *file = fopen(filename, "w");
//    if (!file){
//        perror("Error opening file");
//        return;
//    }
//
//    User *current = users;
//    while (current){
//        fprintf(file, "%s,%s,%s,%s,%s,%d,%ld,%ld\n", current->ID, current->email, current->pwd,
//                current->code, current->name, current->blockCount, current->lastDrawTime,
//                current->lastGetCodeTime);
//        current = current->next;
//    }
//
//    fclose(file);
//}
//
//// findUser
///*User *foundUser = findUser(users, "123456");
//   if (foundUser) {
//        printf("Found User: %s\n", foundUser->name);
//    }
//    */
//User* findUser(User *users, const char *ID){
//    User *current = users;
//    while (current){
//        if (strcmp(current->ID, ID) == 0){
//            return current;
//        }
//        current = current->next;
//    }
//    return NULL; // if not found
//}
//
//
//// deleteUser
//// bool result = deleteUser(&userList, "特定用户的ID");
//User* deleteUser(User *users, const char *ID){
//    User *current = users;
//    User *previous = NULL;
//
//    while (current){
//        if (strcmp(current->ID, ID) == 0){
//            if (previous == NULL){
//                // The deleted node is the head node
//                User* nextNode = current->next;
//                free(current);
//                return nextNode;  // return the new header node
//            }
//            else{
//                // Delete middle or tail nodes
//                previous->next = current->next;
//                free(current);
//                return users;  //return the original header node
//            }
//        }
//        previous = current;
//        current = current->next;
//    }
//
//    return users;  // If the ID is not found, return the original header node
//}
//
//
//// print user list
//void printUserList(User *users){
//    User *current = users;
//    while (current){
//        printf("ID: %s, Email: %s, Name: %s, Block Count: %d, Last Draw Time: %ld, Last Get Code Time: %ld\n",
//               current->ID, current->email, current->name, current->blockCount, current->lastDrawTime, current->lastGetCodeTime);
//        current = current->next;
//    }
//}
//

void parse_json(const char *json, int *row, int *col, int *newValue) {
    // Temporary variables to hold string versions of the numbers
    char rowStr[10], colStr[10], newValueStr[10];

    // Use sscanf to parse out the values
    // This is very basic and assumes the format is exactly as expected.
    sscanf(json, "{\"row\":%9[^,],\"column\":%9[^,],\"color\":%9[^}]}", rowStr, colStr, newValueStr);

    // Convert string numbers to integers
    *row = atoi(rowStr);
    *col = atoi(colStr);
    *newValue = atoi(newValueStr);

    // Debug output
    printf("Parsed JSON: row = %d, column = %d, color = %d\n", *row, *col, *newValue);
}

void updateHtmlcontent(){
	strcpy(htmlContent, gamehtml);
	strcat(htmlContent, scriptTag);
}

void updateDrawingToJson() {
	if(!jsonInitFlag){
		jsonInitFlag = 1;
		jsonStr = malloc(ARRAY_SIZE * ARRAY_SIZE * 4); 
		scriptTag = malloc(102400); 
		 
		printf("JsonStr has been initialized.\n"); 
	}
    int i,j;
    strcpy(jsonStr, "["); 
    for (i = 0; i < ARRAY_SIZE; i++) {
        strcat(jsonStr, "[");
        for (j = 0; j < ARRAY_SIZE; j++) {
            char numStr[4];
            sprintf(numStr, "%d", drawing[i][j]); 
            strcat(jsonStr, numStr);
            if (j < ARRAY_SIZE - 1) {
                strcat(jsonStr, ",");
            }
        }
        strcat(jsonStr, "]");
        if (i < ARRAY_SIZE - 1) {
            strcat(jsonStr, ",");
        }
    }
    strcat(jsonStr, "]");
   	size_t scriptTagSize = 102400;
    // Make sure this is large enough to hold the JSON and script tag
    snprintf(scriptTag, scriptTagSize, 
        "<script>"
        "var colorCache = %s;" // Embed the drawing data as a JavaScript variable
        "</script>", jsonStr);
    //printf("%s\n",scriptTag);
    printf("JsonStr has been updated.\n");
    return;
}

//Model transaction
void initGlobals() {
	// Redirect stdout to log.txt
    freopen(LOG_FILE, "a", stdout);
    printf("\n----------------------\nStarted initGlobals.\n");
	readArrayFromFile(DRAWING_FILE, drawing);
    readDrawingTimeFromFile(DRAWING_TIME_FILE);
    // Setup signal handler for SIGINT
    signal(SIGINT, handle_sigint);
	updateDrawingToJson();
    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&canUpdate, NULL);
    pthread_mutex_init(&client_mutex, NULL);
    nextUpdateTime = time(NULL) + UPDATE_DRAWING_JSON_TIME;
    loadHtml();
}

void *updateJsonThread(void *arg) {
	int times = 0;
    while (!shutdownFlag) {
        sleep(UPDATE_DRAWING_JSON_TIME); // wait serveral seconds
        if(shutdownFlag){
        	return;
		}
		times++; 
		if(times==TIMES_UPDATE_BEFORE_SAVE){
			pthread_mutex_lock(&lock);
			printf("UpdateThread began saving...\n");
			writeDrawingTimeToFile(DRAWING_TIME_FILE);
			writeArrayToFile(DRAWING_FILE, drawing);
			savedTime ++;
			printf("UpdateThread finished saving...\n");
			pthread_mutex_unlock(&lock);
			times = 0;
		}
		
        pthread_mutex_lock(&lock);
        updateFlag = 1;  // startupDate 
        while (time(NULL) < nextUpdateTime) { // wait for the next updateTime
            pthread_cond_wait(&canUpdate, &lock);
        }
        printf("UpdateThread began regenerate drawing array to sent...\n");
        updateJsonTime++;
		// regenerate JsonStr and html
        updateDrawingToJson();
		updateHtmlcontent(); 
		 
        nextUpdateTime = time(NULL) + UPDATE_DRAWING_JSON_TIME;
        updateFlag = 0;  
        printf("UpdateThread finished regenerate drawing array to sent...\n");
        pthread_cond_broadcast(&canUpdate);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

void loadHtml() {
    gamehtml = malloc(1000000); // Ensure this size is large enough to hold the entire HTML content
    char filepath[128];
    snprintf(filepath, sizeof(filepath), "html/game.html"); // Ensure the path is correct

    // Open the HTML file
    FILE *file = fopen(filepath, "rb"); // Use binary mode to ensure unicode characters are read correctly
    if (file == NULL) {
        printf("Failed to load game.html.\n");
        initFlag = 0;
        return;
    } else {
        initFlag = 1;
    }

    // Read the content of game.html into memory
    int totalBytesRead = 0;
    int bytesRead;
    char buffer[4096]; // Make sure the buffer is large enough to hold a chunk of the file
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        memcpy(gamehtml + totalBytesRead, buffer, bytesRead);
        totalBytesRead += bytesRead;
    }
    gamehtml[totalBytesRead] = '\0'; // Ensure the string is null-terminated

    // Initialize htmlContent with sufficient space
    htmlContent = malloc(1000000); // Adjust size accordingly
    if (htmlContent == NULL) {
        // Handle memory allocation error
    }

    // Copy the game HTML content and append the scriptTag
    strcpy(htmlContent, gamehtml);
    strcat(htmlContent, scriptTag);

    fclose(file); // Close the file after reading
}


// Function to add a transaction to a given list
void addTransaction(TransactionNode **list, int row, int col, int newValue,int userId) {
    // Create and initialize a new transaction
    UpdateTransaction newTrans = {row, col, newValue, time(NULL)};
    TransactionNode *newNode = (TransactionNode*)malloc(sizeof(TransactionNode));
    newNode->transaction = newTrans;
    newNode->next = NULL;
    addTransactionTime++;
    if(shutdownFlag){
		return;
	}
    // Append the new transaction to the specified list
    if (*list == NULL) {
        *list = newNode;
		printf("Transaction List %d received [Row: %d, Col: %d, NewValue: %d] by thread %d \n",1-processFlag, row, col, newValue,userId);
    } else {
        TransactionNode *current = *list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
        printf("Transaction List %d received [Row: %d, Col: %d, NewValue: %d] by thread %d \n",1-processFlag, row, col, newValue,userId);
    }
}

// Function to process all transactions in a list
void processTransactions(TransactionNode **list) {
	pthread_mutex_lock(&lock);
    while (*list != NULL) {
        TransactionNode *current = *list; // Get the current transaction
        // Extracting transaction details
        int row = current->transaction.row;
        int col = current->transaction.col;
        int newValue = current->transaction.newValue;
        time_t timestamp = current->transaction.timestamp;
        // Format time
        char timeBuff[30];
        strftime(timeBuff, sizeof(timeBuff), "%Y-%m-%d %H:%M:%S", localtime(&timestamp));

        // Update the drawing array with the new value
        if (row >= 0 && row < ARRAY_SIZE && col >= 0 && col < ARRAY_SIZE) {
            
            // Use provided functions to update drawing and drawingTime
            drawing[row][col] = newValue;
            drawingTime[row][col] = timestamp; // Assuming timestamp is when transaction was created
			broadcast_to_clients(row,col,newValue);
			printf("Transaction List %d processed:[%d, %d] to %d at %s\n", processFlag, row, col, newValue, timeBuff);  
            processTime++;
        } else {
            printf("Invalid transaction coordinates [%d, %d]\n", row, col);
        }

        // Move to next transaction and free the current one
        *list = current->next;
        free(current);
    }
    if (updateFlag) {
        pthread_cond_wait(&canUpdate, &lock);
    }
    pthread_mutex_unlock(&lock);
}



// Thread function to either process or listen for transactions
void *threadWork(void *arg) {
    int threadId = *(int *)arg;
    printf("Thread %d: Started working\n", threadId);

    while (!(shutdownFlag && *currentProcessingList == NULL && *currentListeningList == NULL)){
        // Continue if not shutdown or there are still transactions in either list
        
        if (*currentProcessingList != NULL) {
            processTransactions(currentProcessingList);
        }

        // Swap lists to ensure both lists are processed
        
        TransactionNode **temp = currentProcessingList;
        currentProcessingList = currentListeningList;
        currentListeningList = temp;
		// Update process flag to indicate which list will be processed next
    	//printf("Processed List swaped from %d to %d with AddedTransaction:%d ProcessedTransaction:%d\n",processFlag,1-processFlag,addTransactionTime,processTime);
        processFlag = 1 - processFlag;
	}
    printf("Thread %d: Exiting as shutdown initiated and all transactions processed.\n", threadId);
    endFlag = 1;
    return NULL;
}

//void *threadWork(void *arg) {
//    int threadId = *(int *)arg;
//    printf("Thread %d: Started working\n", threadId);
//    while (!shutdownFlag || *currentProcessingList != NULL) { // Continue if not shutdown or transactions left
//        pthread_mutex_lock(&transactionMutex);
//
//        // Wait until it's this thread's turn
//        while (processFlag != threadId && !shutdownFlag) {
//            pthread_cond_wait(&canProcess, &transactionMutex);
//        }
//
//        // Process all transactions in the current list
//        if (*currentProcessingList != NULL) {
//            processTransactions(currentProcessingList, threadId);
//        }
//
//        // Swap lists
//        TransactionNode **temp = currentProcessingList;
//        currentProcessingList = currentListeningList;
//        currentListeningList = temp;
//
//        processFlag = 1 - threadId; // Give the other thread a turn
//        pthread_cond_signal(&canProcess); // Wake up the other thread
//
//        pthread_mutex_unlock(&transactionMutex);
//
//        // Small delay to allow other thread to take control
//        
//        //usleep(1000);
//    }
//	printf("Thread %d is saving the file.",threadId);
//    //save drawing and drawing time status to file
//    writeDrawingTimeToFile(DRAWING_TIME_FILE);
//	writeArrayToFile(DRAWING_FILE, drawing);
//    printf("Thread %d: Exiting as shutdown initiated and all transactions processed.\n", threadId);
//    return NULL;
//}



//Transaction Test
void* transactionGenerator(void* arg) {
    int id = *(int*)arg;  // Unique id for each generator for logging
    printf("User %d: Connnected\n", id);
	//wait for all test users connected. 
    while(!shutdownFlag) {
        // Sleep for seconds
		sleep(interval);
		//printf("Transaction Generator %d: Running\n", id);
        // Generate random row, col, and newValue
        int row = rand() % ARRAY_SIZE;
        int col = rand() % ARRAY_SIZE;
        int newValue = rand() % (MAX_VALUE + 1);  // newValue is between 0 and MAX_VALUE

        // Determine which list to add the transaction to
        TransactionNode** targetList = (processFlag == 0) ? currentListeningList : currentProcessingList;

        // Add the transaction
        pthread_mutex_lock(&transactionMutex);
        addTransaction(targetList, row, col, newValue, id);
        pthread_mutex_unlock(&transactionMutex);
    }
	//printf("User %d disconnected.", id);
    return NULL;
}


//Model-drawingTime
void updateDrawingTime(const char *filename, int row, int col, time_t newTime) {
    // Check the validity of the row and column
    if (row < 0 || row >= ARRAY_SIZE || col < 0 || col >= ARRAY_SIZE) {
        fprintf(stderr, "Row or column out of range\n");
        return;
    }

    // Update the array in memory
    drawingTime[row][col] = newTime;

    // Update the corresponding value in the file
    FILE *file = fopen(filename, "r+b"); // Open the file for reading and writing
    if (file == NULL) {
        perror("Error opening file for updating");
        return;
    }

    // Locate the correct position in the array
    long offset = (long)(row * ARRAY_SIZE + col) * sizeof(time_t);
    if (fseek(file, offset, SEEK_SET) != 0) {
        perror("Error seeking in file");
        fclose(file);
        return;
    }

    // Write the new time value
    if (fwrite(&newTime, sizeof(time_t), 1, file) != 1) {
        perror("Error writing to file");
        fclose(file);
        return;
    }

    fclose(file);
}

void initializeAndWriteDrawingTime(const char *filename, time_t initialTime) {
    int i, j;
    printf("DrawingTime.txt is initializing...\n"); 
    for (i = 0; i < ARRAY_SIZE; i++) {
        for (j = 0; j < ARRAY_SIZE; j++) {
            drawingTime[i][j] = initialTime;
        }
    }
    writeDrawingTimeToFile(filename);
    printf("Successfully initialize DrawingTime.\n");
}

void readDrawingTimeFromFile(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Initializing new DrawingTime file.\n");
        time_t initialTime = time(NULL); 
        initializeAndWriteDrawingTime(filename, initialTime);
		file = fopen(filename, "rb");
    }
	
    if (file == NULL) {
        perror("Failed to open file for reading\n");
        return;
    }
    // In the readDrawingTimeFromFile function:
	fseek(file, 0, SEEK_END);
	long fileSize = ftell(file);
	long expectedSize = ARRAY_SIZE * ARRAY_SIZE * sizeof(time_t);
	if (fileSize != expectedSize) {
    	fprintf(stderr, "File size mismatch! Expected: %ld, Got: %ld\n", expectedSize, fileSize);
    	// Handle error or reinitialize file here
	}
	fseek(file, 0, SEEK_SET); // Reset the file pointer to the beginning of the file

	int i,j;
    // Read file
    for (i = 0; i < ARRAY_SIZE; i++) {
        for (j = 0; j < ARRAY_SIZE; j++) {
            time_t value;
            if (fread(&value, sizeof(time_t), 1, file) != 1) {
                if (feof(file)) {
                    fprintf(stderr, "Unexpected end of file at [%d][%d]\n", i, j);
                } else {
                    perror("Error reading from file\n");
                }
                printf("Failed at position [%d][%d], total items expected to read: %d\n", i, j, ARRAY_SIZE * ARRAY_SIZE);
                fclose(file);
                return;
            }
            drawingTime[i][j] = value;
        }
    }
    fclose(file);
    printf("Successfully read drawingTime from file\n");
}

void writeDrawingTimeToFile(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("Failed to open file for writing\n");
        return;
    }
	int i,j;
    for (i = 0; i < ARRAY_SIZE; i++) {
        for (j = 0; j < ARRAY_SIZE; j++) {
            if (fwrite(&drawingTime[i][j], sizeof(time_t), 1, file) != 1) {
                perror("Error writing to file");
                fclose(file);
                return;
            }
        }
    }
    printf("Successfully write DrawingTime to file.\n");
    fclose(file);
}


//Model-drawing

//If connected to a database, this function can be used. 
void updateArray(const char *filename, int row, int col, int newValue){
    // Check the validity of the row and column
    if (row < 0 || row >= ARRAY_SIZE || col < 0 || col >= ARRAY_SIZE){
        fprintf(stderr, "Row or column out of range\n");
        return;
    }

    // Check the validity of the new value
    if (newValue < 0 || newValue > MAX_VALUE){
        fprintf(stderr, "New value out of range\n");
        return;
    }

    // Update the array in memory
    drawing[row][col] = newValue;

    // Update the corresponding value in the file
    FILE *file = fopen(filename, "r+b"); // Open the file for reading and writing
    if (file == NULL)
    {
        perror("Error opening file for updating");
        exit(EXIT_FAILURE);
    }

    // Locate the correct position in the array
    long offset = (long) (row * ARRAY_SIZE + col) * sizeof(unsigned char);
    if (fseek(file, offset, SEEK_SET) != 0)
    {
        perror("Error seeking in file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Write the new value
    unsigned char value = (unsigned char)newValue;
    if (fwrite(&value, sizeof(unsigned char), 1, file) != 1)
    {
        perror("Error writing to file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    
}


void initializeAndWriteArray(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]){
	printf("DrawingArray is initializing...\n");
	int i,j;
    for (i = 0; i < ARRAY_SIZE; i++)
    {
        for (j = 0; j < ARRAY_SIZE; j++)
        {
            array[i][j] = MAX_VALUE;
        }
    }
    writeArrayToFile(filename, array);
    printf("Successfully initialize DrawingArray.\n");
}


//Initiate the Array
void writeArrayToFile(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]){
    FILE *file = fopen(filename, "wb");
    if (file == NULL)
    {
        initializeAndWriteArray(filename, array);
        exit(EXIT_FAILURE);
    }
	int i,j;
    for (i = 0; i < ARRAY_SIZE; i++)
    {
        for (j = 0; j < ARRAY_SIZE; j++)
        {
            if (array[i][j] > MAX_VALUE)
            {
                fprintf(stderr, "Array value out of range at [%d][%d]\n", i, j);
                fclose(file);
                exit(EXIT_FAILURE);
            }
            unsigned char value = (unsigned char)array[i][j];
            if (fwrite(&value, sizeof(unsigned char), 1, file) != 1)
            {
                perror("Error writing to file");
                fclose(file);
                exit(EXIT_FAILURE);
            }
        }
    }

    fclose(file);
    printf("Successfully write Drawing to file.\n");
}

void readArrayFromFile(const char *filename, int array[ARRAY_SIZE][ARRAY_SIZE]){
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        // The file does not exist
        initializeAndWriteArray(filename, array);
        printf("Successfully initialize the drawing.txt.\n");
    }
	file = fopen(filename, "rb");
	if (file == NULL)
    {
        printf("Failed to read data from drawing.txt.\n");
        return;
    }
    // Check if the size of file is correct.
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (fileSize != (long) ARRAY_SIZE * ARRAY_SIZE * sizeof(unsigned char))
    {
        fclose(file);
        initializeAndWriteArray(filename, array);
        return;
    }
	int i,j;
    // Read file
    for (i = 0; i < ARRAY_SIZE; i++)
    {
        for (j = 0; j < ARRAY_SIZE; j++)
        {
            unsigned char value;
            if (fread(&value, sizeof(unsigned char), 1, file) != 1)
            {
                if (feof(file))
                {
                    fprintf(stderr, "Unexpected end of file at [%d][%d]\n", i, j);
                }
                else
                {
                    perror("Error reading from drawing.txt");
                }
                fclose(file);
                exit(EXIT_FAILURE);
            }
            if (value > MAX_VALUE)
            {
                fprintf(stderr, "Invalid value in file at [%d][%d]\n", i, j);
                fclose(file);
                exit(EXIT_FAILURE);
            }
            array[i][j] = (int)value;
        }
    }

    fclose(file);
    printf("Successfully read drawingArray from file\n");
}



//Makes the given socket file descriptor non-blocking.
int make_socket_non_blocking(int sfd){
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1)
    {
        perror("fcntl");
        return -1;
    }
    flags |= O_NONBLOCK;
    if (fcntl(sfd, F_SETFL, flags) == -1)
    {
        perror("fcntl");
        return -1;
    }
    return 0;
}

//Runs the main server loop using epoll to handle multiple connections.
void run_server(int sfd){
	printf("The server is starting...\n");
    int efd,i;
    setup_epoll(sfd, &efd);
    struct epoll_event events[MAX_EVENTS];
	
    while (!shutdownFlag)
    {
    	
        int n = epoll_wait(efd, events, MAX_EVENTS, -1);
		printf("The server is running...\n");
        for (i = 0; i < n; i++)
        {
            if (events[i].data.fd == sfd)
            {	
            	printf("Handle new connection:%d %d\n",sfd,efd);
                handle_new_connection(sfd, efd);
            }
            else
            {
                handle_client_request(events[i].data.fd);
            }
        }
    }
	printf("The server has closed.\n");
}

//Initializes and returns a server socket bound to the given port.
int initialize_server(int port){
    // Create a socket for the IPv4 network and using TCP/IP
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1)
    {
        perror("socket"); // Log error if socket creation fails
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse the address. Useful for re-binding the socket during quick restarts
    int opt = 1;
    if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("setsockopt"); // Log error if setting socket options fails
        exit(EXIT_FAILURE);
    }

    // Define the server address: IPv4, listening on all interfaces, on the specified port
    struct sockaddr_in addr =
    {
        .sin_family = AF_INET,          // Use IPv4
        .sin_addr.s_addr = htonl(INADDR_ANY), // Listen on all network interfaces
        .sin_port = htons(port),        // Set the port to listen on
    };

    // Bind the socket to the specified address and port
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1)
    {
        perror("bind"); // Log error if binding fails
        close(sfd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections on the socket
    if (listen(sfd, SOMAXCONN) == -1)
    {
        perror("listen"); // Log error if listening fails
        close(sfd);
        exit(EXIT_FAILURE);
    }

    // Set the socket to non-blocking mode to allow handling multiple connections
    make_socket_non_blocking(sfd);
    return sfd; // Return the socket file descriptor
}


//Sets up epoll instance and registers the server socket.
void setup_epoll(int sfd, int *efd){
    *efd = epoll_create1(0);
    if (*efd == -1)
    {
        perror("epoll_create");
        exit(EXIT_FAILURE);
    }

    struct epoll_event event =
    {
        .data.fd = sfd,
        .events = EPOLLIN | EPOLLET,
    };

    if (epoll_ctl(*efd, EPOLL_CTL_ADD, sfd, &event) == -1)
    {
        perror("epoll_ctl");
        close(sfd);
        exit(EXIT_FAILURE);
    }
}

//Handles new incoming connections on the server socket
void handle_new_connection(int sfd, int efd){
    while (1)
    {
        struct sockaddr in_addr;
        socklen_t in_len = sizeof(in_addr);

        // Accept a new connection
        int infd = accept(sfd, &in_addr, &in_len);
        if (infd == -1)
        {
            // If no more incoming connections are present, break the loop
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
            else
            {
                perror("accept"); // Log error if accept fails for reasons other than EAGAIN or EWOULDBLOCK
                break;
            }
        }

        // Make the socket non-blocking to handle multiple connections
        make_socket_non_blocking(infd);

        struct epoll_event event =
        {
            .data.fd = infd,            // Set file descriptor for the new connection
            .events = EPOLLIN | EPOLLET, // Set event type to EPOLLIN (Read operation) and use Edge Triggered mode
        };

        // Add the new file descriptor to the epoll instance
        if (epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) == -1)
        {
            perror("epoll_ctl"); // Log error if epoll_ctl fails
            close(infd);         // Close the file descriptor before breaking
            break;
        }
    }
}


// Returns true if the request is valid and parsed successfully, false otherwise.
bool parse_request(const char *request, char *method, char *filename){
    const char *method_end = strchr(request, ' ');
    const char *path_start = method_end ? method_end + 1 : NULL;
    const char *path_end = path_start ? strchr(path_start, ' ') : NULL;

    if (method_end && path_start && path_end)
    {
        size_t method_len = method_end - request;
        size_t len = path_end - path_start;
        strncpy(method, request, method_len);
        method[method_len] = '\0';
        strncpy(filename, path_start, len);
        filename[len] = '\0';

        const char *http_version = path_end + 1;
        if (strncmp(http_version, "HTTP/1.1", 8) == 0)
        {
            return true;
        }
    }
    return false;
}

// Handle POST request
void handle_post_request(int client_fd, char* body) {
    // Parse JSON data from body
    int row, col, newValue;
    parse_json(body, &row, &col, &newValue);
    // Determine target list based on processFlag
    TransactionNode** targetList = (processFlag == 0) ? currentListeningList : currentProcessingList;
    // Lock the mutex before modifying the list
    pthread_mutex_lock(&transactionMutex);
    // Add transaction
    addTransaction(targetList, row, col, newValue, client_fd);
    // Unlock the mutex
    pthread_mutex_unlock(&transactionMutex);

    // Update last activity time for the client
    update_last_activity(client_fd);

    // Send a successful response back to the client
    const char *response = "{\"status\": \"success\", \"message\": \"Painting data received\"}";
    char header[1024];
    snprintf(header, sizeof(header), 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %ld\r\n\r\n%s", strlen(response), response);
    send(client_fd, header, strlen(header), 0);
}




// Send a response to the client based on the requested filename
void send_response(int client_fd, const char *method, const char *filename){
    char filepath[128];
    const char *content_type;
	char header[1024];
	
    if(!initFlag){
    	send(client_fd, http404Response, strlen(http404Response), 0);
    	return;
	}
    // Construct the file path based on the request
    // Determine the content type and construct the file path based on the request
    if(strcmp(method, "GET") == 0) {
        if(strcmp(filename, "/") == 0 || strcmp(filename, "/index.html") == 0|| strcmp(filename, "/main.html") == 0 ||strcmp(filename, "/game.html") == 0) {
			pthread_mutex_lock(&lock);
			if (updateFlag) {
        		pthread_cond_wait(&canUpdate, &lock);
    		}
			
			content_type = "text/html; charset=UTF-8";
			snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", content_type);
			send(client_fd, header, strlen(header), 0);
			send(client_fd, htmlContent, strlen(htmlContent), 0);
			pthread_mutex_unlock(&lock);
			return;
        }else if (strncmp(filename, "/html/", 6) == 0) {
	    	//snprintf(filepath, sizeof(filepath), "%s", filename + 1); // +1 to skip the first '/'
    		//content_type = "text/html";
		} else if (strncmp(filename, "/css/", 5) == 0) {
            snprintf(filepath, sizeof(filepath), "css%s", filename + 4); // +4 to skip "/css"
            content_type = "text/css";
        } else if (strncmp(filename, "/js/", 4) == 0) {
            snprintf(filepath, sizeof(filepath), "js%s", filename + 3); // +3 to skip "/css"
            content_type = "application/javascript";
        } else {
            // If the file is not found in any specified directories, send 404 Not Found error
            char custom_404_message[1024];
            snprintf(custom_404_message, sizeof(custom_404_message), "404 Not Found: %s", filename);
            send_error_response(client_fd, "404 Not Found", custom_404_message);
            return;
        }

        FILE *file = fopen(filepath, "rb");
        if(file == NULL) {
            char custom_404_message[1024];
            snprintf(custom_404_message, sizeof(custom_404_message), "404 Not Found: %s", filename);
            send_error_response(client_fd, "404 Not Found", custom_404_message);
            return;
        }

        // Send HTTP response header
        char header[1024];
        snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", content_type);
        send(client_fd, header, strlen(header), 0);

        // Read and send the file content in chunks
        char buffer[4096];
        int bytes_read;
        while((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(client_fd, buffer, bytes_read, 0);
        }
        fclose(file);
    }else{
        // Handle unsupported methods
        send_error_response(client_fd, "501 Not Implemented", "Not Implemented");
    }
}

//Logs an error message to stderr
void log_error(const char *msg){
    // Logs the error message to stderr. This function can be expanded to include more sophisticated error handling.
    perror(msg);

    // Placeholder for additional error handling features.
    // For example, you might want to add logging to a file, or include more detailed error information.
    // TODO: Add additional error handling and logging features here in future expansions.
}

//Sends a custom error response to the client.
void send_error_response(int client_fd, const char *status, const char *message){
    char response[1024];
    snprintf(response, sizeof(response), "HTTP/1.1 %s\r\nContent-Length: %ld\r\n\r\n%s", status, strlen(message), message);
    send(client_fd, response, strlen(response), 0);
}

// Handles the client's request by reading it, processing it, and sending a response.
void handle_client_request(int client_fd) {
	char buf[4096]; // This buffer should be large enough to hold the request header.
    int count = read(client_fd, buf, sizeof(buf) - 1);
    if (count > 0) {
        buf[count] = '\0'; // Null-terminate whatever we've read.
        char method[10]; // To store method type like GET or POST
        char filename[256]; // To store the filename or path from the request
		if (strstr(buf, "Upgrade: websocket") && strstr(buf, "Connection: Upgrade")) {
        	handle_websocket_handshake(client_fd, buf); // handle WebSocket hanshake
    		//close(client_fd);
			return;	
		} 
        if (parse_request(buf, method, filename)) {
            if (strcmp(method, "POST") == 0) {
                // Find content length header to determine the length of the body
                const char* contentLenHeader = strstr(buf, "Content-Length: ");
                int contentLength = 0;
                if (contentLenHeader) {
                    sscanf(contentLenHeader, "Content-Length: %d", &contentLength);
                }
                // Read the body of the request
                char* body = strstr(buf, "\r\n\r\n") + 4; // Move past the header
                if (body) {
                    printf("Body: %.*s\n", contentLength, body); // Print body
                    handle_post_request(client_fd, body); 
        			pthread_mutex_lock(&lock);
					if (updateFlag) {
        				pthread_cond_wait(&canUpdate, &lock);
    				}
					char header[1024];
					char* content_type = "text/html";
					snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", content_type);
					send(client_fd, header, strlen(header), 0);
					send(client_fd, htmlContent, strlen(htmlContent), 0);
					pthread_mutex_unlock(&lock);
					return;
                } else {
                    // If body wasn't found or contentLength is incorrect
                    send_error_response(client_fd, "400 Bad Request", "Bad Request");
                }
            } else if (strcmp(method, "GET") == 0) {
                // Handle GET request
                send_response(client_fd, method, filename);
            }
        } else {
            // If request is not valid
            send_error_response(client_fd, "400 Bad Request", "Bad Request");
        }
    } else if (count == -1 && errno != EAGAIN) {
        log_error("read");
        send_error_response(client_fd, "500 Internal Server Error", "Server Error");
    }
    close(client_fd);
}

void add_client(int client_fd) {
    if (client_count < MAX_CLIENTS) {
        ws_client_t new_client;
        new_client.fd = client_fd;
        new_client.state = WS_STATE_CONNECTED;
        new_client.lastActivity = time(NULL); 
        clients[client_count++] = new_client;
    }
}

void remove_client(int client_fd) {
    int i;
	for (i = 0; i < client_count; ++i) {
        if (clients[i].fd == client_fd) {
            clients[i] = clients[--client_count];
            close(client_fd);
            return;
        }
    }
}

void handle_websocket_handshake(int client_fd, const char *buf) {
    // 1. Parsing Sec WebSocket Key from Request
    char sec_websocket_key[256]; // Assuming that the Sec WebSocket Key does not exceed 255 characters
    sscanf(strstr(buf, "Sec-WebSocket-Key: ") + 19, "%s", sec_websocket_key);
    // 2. joint magic GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    strcat(sec_websocket_key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

    // 3. SHA1 hash the results
    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)sec_websocket_key, strlen(sec_websocket_key), hash);

    // 4. SHA1 hash the results
    char base64EncodeOutput[28]; // WebSocket accepts fixed key length
    EVP_EncodeBlock((unsigned char*)base64EncodeOutput, hash, SHA_DIGEST_LENGTH);

    // 5. Building WebSocket handshake response
    char response[256];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n\r\n", base64EncodeOutput);

    // 6. send handshaking response and add client;
    send(client_fd, response, strlen(response), 0);
    add_client(client_fd);
}


// broadcast messages to all the WebSocket clients
void broadcast_to_clients(int row, int col, int newValue) {
	int i,fd;
    for(i = 0; i < client_count; ++i) {
    	fd = clients[i].fd;
        send_websocket_message(fd, row,col,newValue);
    }
}


void send_websocket_message(int client_fd, int row, int col, int newValue) {
    // Building WebSocket handshake response {"row":y,"column":x,"color":n}
    char message[256];
    sprintf(message, "{\"row\":%d,\"column\":%d,\"color\":%d}", row, col, newValue);

	//Simplify processing without considering message segmentation
	//Build WebSocket text frame header (0x81 represents the last frame, text frame)
    unsigned char frameHeader[2] = {0x81, strlen(message)};

    // Send frame header
    send(client_fd, frameHeader, 2, 0);

    // Send frame payload (message itself)
    send(client_fd, message, strlen(message), 0);
}

void handle_sigint(int sig) {

    printf("\nCaught signal %d, initiating graceful shutdown...\n", sig);
    shutdownFlag = 1; // Set shutdown flag
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&canUpdate);
 
	//free(htmlContent);
	//free(jsonStr); 
	//free(scriptTag);
	//free(http404Response);
	//free(gamehtml);
}

// Create and detach transaction generator threads
void gennerateTestUsers(int testNum,int newinterval,int sleepTime){
	interval = newinterval;
	sleep(sleepTime); 
	int i;
    pthread_t generators[testNum];
    int* genIds = malloc(testNum * sizeof(int));
    for (i = 0; i < testNum; i++) {
        genIds[i] = i + 1; // Assign an id to each generator
        pthread_create(&generators[i], NULL, transactionGenerator, &genIds[i]);
        pthread_detach(generators[i]);
	}
}

void update_last_activity(int client_fd) {
    pthread_mutex_lock(&client_mutex); // Lock to synchronize access
	int i;
    bool found = false;
    for (i = 0; i < client_count; ++i) {
        if (clients[i].fd == client_fd) {
            found = true;
            // Check if the file descriptor is still valid
            if (fcntl(clients[i].fd, F_GETFD) != -1 || errno != EBADF) {
                // File descriptor is valid, update activity time
                clients[i].lastActivity = time(NULL);
                clients[i].state = WS_STATE_CONNECTED; // Ensure that the status is connected
            } else {
                // If the file descriptor is invalid, remove the client and close the connection
                remove_client(client_fd);
            }
            break;
        }
    }

    // If the current client_ fd is not in the active list, add it
    if (!found) {
        add_client(client_fd);
    }

    pthread_mutex_unlock(&client_mutex); // unlock
}




void* cleanup_task(void* arg){
    while (!shutdownFlag) {
        sleep(TIME_TO_CLEANUP); 

        time_t now = time(NULL);
        pthread_mutex_lock(&client_mutex); //  Assuming there is a mutex for client arrays
		int i;
        for (i = 0; i < client_count;) {
            // If the client is inactive for more than 5 minutes, it is considered disconnected
            if (difftime(now, clients[i].lastActivity) > 300) {
                remove_client(clients[i].fd); // Remove client
            } else {
                ++i;
            }
        }

        pthread_mutex_unlock(&client_mutex);
    }
    return NULL;
}


int main() {
    clock_t start, end;
    double cpu_time_used;
    start = clock();
  	initGlobals();
  	//thread to updateJsonThread  
    pthread_t updater;
    pthread_create(&updater, NULL, updateJsonThread, NULL);
	pthread_detach(updater); 	
    
    //thread to cleanup clients which are not active
    pthread_t cleanupThread;
    pthread_create(&cleanupThread, NULL, cleanup_task, NULL);
    pthread_detach(cleanupThread); 
    
    int sfd = initialize_server(PORT);
	
    // Create worker threads
    pthread_t workerThreads[2];
    int id1 = 0, id2 = 1;
	pthread_create(&workerThreads[0], NULL, threadWork, (void *)&id1);
	pthread_detach(workerThreads[0]);   // detach the thread
	//	pthread_create(&workerThreads[1], NULL, threadWork, (void *)&id2);
	//	pthread_detach(workerThreads[1]); 
	
	//Generate Test cases(user_num,interval,sleepTime)
	gennerateTestUsers(100,1,3); 
	
    // Initiate server loop
    run_server(sfd); // Modify to exit based on shutdownFlag
    
    // Close sockets and free resources
    close(sfd); // Close server socket
	printf("Close server socket successfully.\n");
    // Ensure all logs are written and then close log file
    fflush(stdout);
    int i;
    while(!endFlag){
	    printf("The Transaction List is still being processd(%ds)\n",i);
	    printf("AddedTransaction:%d\nProcessedTransaction:%d\n",addTransactionTime,processTime);
	    sleep(1);
	    i++;
	}
	printf("Saving the file before closing...\n");
    //save drawing and drawing time status to file
    writeDrawingTimeToFile(DRAWING_TIME_FILE);
	writeArrayToFile(DRAWING_FILE, drawing);
	end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Exiting as shutdown initiated and all transactions processed.\n");
	printf("\n-------------------------------\nFinished with following information: \nRunning Time:%f s\nAddedTransaction:%d ProcessedTransaction:%d \nUpdated ColourCacheTime:%d SavedFileTime:%d\n",cpu_time_used,addTransactionTime,processTime,updateJsonTime,savedTime);
	fclose(stdout);
	return 0;
	
}
