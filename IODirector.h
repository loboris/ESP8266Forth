/*
 * Input / Output Director for Yaffa Forth for the 32 bit ESP8266
 *
 * This code controls input and output to/from Yaffa Forth.
 * Sources of data include: USB Serial input, named file input and network input
 * Data can be output to USB Serial or the network. The Forth word
 * serialIO makes the IODirector take its input and output from the USB Serial
 * connection. The netIO word makes the IODirector take its input and output
 * from the network. A telnet connection to port SERVER_PORT (21) can then
 * be used to interact with Yaffa Forth over the network. The Forth word load
 * is used to load a file of Forth code from an SD card.
 *
 * Concept, design and implementation by: Craig A. Lindley
 * Version: 0.6
 * Last Update: 04/30/2016
 */

#ifndef IODIRECTOR
#define IODIRECTOR

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))

// The size of the input and output circular queues
#define QUEUE_SIZE 2048

// Three supported channels for I/O
enum CHANNELS {SERIAL_IO, FILE_IO, NET_IO};

// Server port IODirector listens on
const int SERVER_PORT = 21;

// The networking components
WiFiServer server(SERVER_PORT);
WiFiClient serverClient;

// IODirector class definition
class IODirector {
  public:

    // Class constructor
    IODirector() {
      // Initialize in and out queues
      initializeQueue(&inQueue);
      initializeQueue(&outQueue);

      // Indicate a file is not yet open
      fileOpen = false;

      // Default channel is serial
      currentChannel = SERIAL_IO;
    }

    // Called periodically to transfer data within the IODirector
    void processQueues(void) {
      int charsAvailable, charCount;

      // Determine how much room in the inQueue is available
      int roomInQueue = room(&inQueue);

      switch (currentChannel) {

        case SERIAL_IO:
          // Process in coming data
          charsAvailable = Serial.available();
          if (charsAvailable != 0) {
            charCount = min(roomInQueue, charsAvailable);
            for (int i = 0; i < charCount; i++) {
              put(&inQueue, Serial.read());
            }
          }
          // Process out going data
          charCount = count(&outQueue);
          for (int i = 0; i < charCount; i++) {
            Serial.write(get(&outQueue));
          }
          break;

        case FILE_IO:
#ifdef HAS_SD_CARD
          // Process in coming data
          charsAvailable = forthFile.available();
          if (charsAvailable != 0) {
            charCount = min(roomInQueue, charsAvailable);
            for (int i = 0; i < charCount; i++) {
              char ch = forthFile.read();
              // Ignore LF characters in file, if any
              if (ch == 0x0A) {
                continue;
              }
              put(&inQueue, ch);
            }
          } else  {
            fileHousekeeping();
          }
          // Throw away all output data during FILE_IO
          charCount = count(&outQueue);
          for (int i = 0; i < charCount; i++) {
            get(&outQueue);
          }
#endif
          break;

        case NET_IO:
          if (! serverClient.connected()) {
            // Wait for a connection to be made
            serverClient = server.available();
          }

          // Process in coming data
          charsAvailable = serverClient.available();
          if (charsAvailable != 0) {
            charCount = min(roomInQueue, charsAvailable);
            for (int i = 0; i < charCount; i++) {
              put(&inQueue, serverClient.read());
            }
          }
          // Process out going data
          charCount = count(&outQueue);
          for (int i = 0; i < charCount; i++) {
            serverClient.write(get(&outQueue));
          }
          break;
      }
    }

#ifdef HAS_SD_CARD

    // Set the Forth file to be loaded. Name must be in 8.3 format
    bool setFile(const char *filename) {
      // Check the filename string's length
      int len = strlen(filename);
      if ((len == 0) || (len > 12)) {
        Serial.println("Filename of form: XXXXXXXX.YYY required\r\n");
        return false;
      }

      // Attempt to open the specified file
      forthFile = SD.open(filename);
      if (! forthFile) {
        // File open error
        Serial.print("Error opening file: ");
        Serial.println(filename);
        return false;
      }

      // Success opening file
      fileOpen = true;

      // Save interrupted channel for later restoration
      interruptedChannel = currentChannel;
      return true;
    }

    // Cleanup after Forth file loading. Also called on compilation error anytime
    // an exception is raised.
    void fileHousekeeping(void) {
      if (fileOpen) {
        forthFile.close();

        fileOpen = false;

        // Restore interrupted channel
        currentChannel = interruptedChannel;
      }
    }

    // Inject autorun filename into inQueue
    void injectAutoRunFile(void) {
      char buffer[30];

      memset(buffer, 0, sizeof(buffer));
      strcpy(buffer, "load autorun.frt\r");

      // Copy forth string into inQueue
      int len = strlen(buffer);
      for (int i = 0; i < len; i++) {
        put(&inQueue, buffer[i]);
      }
    }
#endif

    // Select the channel to do I/O with
    void selectChannel(enum CHANNELS channel) {
      // Save new channel
      currentChannel = channel;

      // Network I/O selected ?
      if (channel == NET_IO) {
        if ((! serverClient) || (! serverClient.connected())) {
          // If previous connection, disconnect
          if (serverClient) {
            serverClient.stop();
          }
        }
      }
    }

    // See if input queue has any chars to return
    int available(void) {
      processQueues();
      return inQueue.count;
    }

    // Read a char from the input queue
    char read(void) {
      return get(&inQueue);
    }

    // Write a character to the output queue
    void write(char ch) {
      put(&outQueue, ch);
    }

    // Print signed integer
    int printInt(int32_t i, int format = DEC) {
      char buffer[12];
      memset(buffer, 0, sizeof(buffer));

      switch (format) {
        case BIN:
          itoa (i, buffer, 2);
          break;

        case OCT:
          sprintf(buffer, "%o", i);
          break;

        case DEC:
          sprintf(buffer, "%d", i);
          break;

        case HEX:
          sprintf(buffer, "%x", i);
          break;
      }
      int len = strlen(buffer);
      for (int i = 0; i < len; i++) {
        put(&outQueue, buffer[i]);
      }
      processQueues();
      return len;
    }

    // Print unsigned integer
    int printUInt(uint32_t i, int format = DEC) {
      char buffer[12];
      memset(buffer, 0, sizeof(buffer));

      switch (format) {
        case BIN:
          itoa (i, buffer, 2);
          break;

        case OCT:
          sprintf(buffer, "%o", i);
          break;

        case DEC:
          sprintf(buffer, "%u", i);
          break;

        case HEX:
          sprintf(buffer, "%x", i);
          break;
      }
      int len = strlen(buffer);
      for (int i = 0; i < len; i++) {
        put(&outQueue, buffer[i]);
      }
      processQueues();
      return len;
    }

    // Print a string
    int printString(const char *str) {
      int len = strlen(str);
      for (int i = 0; i < len; i++) {
        put(&outQueue, str[i]);
      }
      processQueues();
      return len;
    }

  private:
    bool fileOpen;

#ifdef HAS_SD_CARD
    File forthFile;
#endif

    enum CHANNELS currentChannel;
    enum CHANNELS interruptedChannel;

    // Circular queue type definition
    typedef struct queue {
      int size;
      int start;
      int count; // Number of elements in queue
      char elements[QUEUE_SIZE];
    } queue_t;

    // Initialize the queue data structure
    void initializeQueue(queue_t *queue) {
      queue->size = QUEUE_SIZE;
      queue->start = 0;
      queue->count = 0;
    }

    // Determine if the queue is full
    bool full(queue_t *queue) {
      return (queue->count == queue->size) ? true : false;
    }

    // Determine if the queue is empty
    bool empty(queue_t *queue) {
      return (queue->count == 0) ? true : false;
    }

    // Determine how much room is available in the queue
    int room(queue_t *queue) {
      return (queue->size - queue->count);
    }

    // Determine how much data is in the queue
    int count(queue_t *queue) {
      return queue->count;
    }

    // Put a character into a queue
    void put(queue_t *queue, char ch) {
      int index;
      if (full(queue)) {
        Serial.println("Queue full\n");
      } else {
        index = queue->start + queue->count++;
        if (index >= queue->size) {
          index -= queue->size;
        }
        queue->elements[index] = ch;
      }
    }

    // Get a character from a queue
    char get(queue_t *queue) {
      char ch;
      if (empty(queue)) {
        Serial.println("Queue empty\n");
        return 0;
      } else {
        ch = queue->elements[queue->start];
        queue->start++;
        queue->count--;
        if (queue->start == queue->size) {
          queue->start = 0;
        }
        return ch;
      }
    }

    // Input and Output Queues
    queue_t inQueue;
    queue_t outQueue;
};

#endif


