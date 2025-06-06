=======================================
# PROGRAM NAME: QR Code Decoder Program
=======================================

## PROGRAM OVERVIEW
====================
    This program provides a TCP-based QR Code Server and Client implementation in C, designed to:
       - Handle the decoding of QR code images uploaded by clients.
       - Support rate-limiting, timeout handling, and concurrency to prevent resource abuse.
       - Log server events such as connections, disconnections, errors, and rate-limit violations.
    The purpose is to familiarize with socket programming while exploring practical applications of networking and image decoding.
----------------------------------------------------------------------------------------------------------------------------------

## FEATURES
============
# QR Code Server:
       1. Single-Threaded Base: Handles connections and processes requests sequentially.
       2. QR Code Decoding: Decodes QR code files using the ZXing Java library.
       3. Binary Transmission Support: Accepts QR code image files from clients.
       4. Concurrency: Supports multiple clients using threads for parallel processing, supports 3 clients.
       5. Rate-Limiting: Restricts the number of requests per client within a given timeframe. 
       6. Timeout Handling: Disconnects idle clients after a specified timeout period.  Default: 90 seconds.
       7. Logging: Logs all events, including connections, disconnections etc.
       8. Security Features: Implements safeguards such as file size limits and protection against invalid data.

# QR Code Client:
       1. Connects to the server via TCP.
       2. Sends files (binary QR code images) to the server.
       3. Receives and displays server responses, including decoded QR code data or error messages.
----------------------------------------------------------------------------------------------------------------

## REQUIREMENTS
================
#  Softwares:
       1. Operating System: Linux (Recommended for socket programming and Java execution)
       2. Compiler: GCC (with pthread support)
       3. Java Runtime Environment: For executing the ZXing Java library
                                
# Libraries Used: 
       1. Native C Libraries
       2. ZXing Java library: Used to decode QR codes by server.
                               - Download the javase.jar and core.jar files from the provided project resources.
                               - Ensure Java is installed (install by this command for Ubuntu: sudo apt install default-jre).
----------------------------------------------------------------------------------------------------------------------------

## PROGRAM STRUCTURE
=====================
      1. QRServer.c: Server Source Code      
      2. QRClient.c: Client Source Code  
      3. Makefile: Script for compiling the program. 
      4. core.jar & javase.jar: ZXing library files for generating and decoding QR codes
      5. test1.png,test2.png,test3.png,test4.png,test5.png,test6.png:  QR Code PNG file for testing
      6. admin_log.txt: Log file created by the server for administrative log reports
      7. README.txt: Documentation
-------------------------------------------------------------------------------------------------------------------------

## USAGE INSTRUCTION
=====================
# Compilation and Execution: 
       All source code, Makefile, core.jar & javase.jar should be on both server and client OS directory for compilation.
          - Compile program on Server and Client OS by the command: make all
          - Compilation generates executable file for Server and Client i.e QRServer and QRClient

# Step 1 Start the Server:
        Run the server program: ./QRServer -PORT <port-number> -MAX_USERS <max-clients> -RATE_MSGS <limit> -RATE_TIME <timeframe> -TIME_OUT <timeout>
        
        Server supports 3c oncurrency client connect at a time
        The server listens on the default port 3600

        e.g. => To run on specific port and other commands options: ./QRServer -PORT 2001 -MAX_USERS 3 -RATE_MSGS 5 -RATE_TIME 20 -TIME_OUT 30
 
# Step 2 Run the Client:
          Run the client program: ./QRClient <server_ip> <server_port>
          Make sure to match the port number on Server and Client.
                           e.g. : ./QRClient 10.25.5.1 2001
    
# Step 3 Send a File:
          The client will prompt these options once connected => Enter 'close' to disconnect, 'shutdown' to turn off server, or a QR code filename:
          choose the commands option provided.
          To Send QR code for decoding just enter the path of the QR code file: <filename>.png
                                                                          e.g.:  test1.png
          
# Step 4 Receive Server Response:
       The client receives the server's response, which will be Decoded QR code URL (if valid), or other messages.
-----------------------------------------------------------------------------------------------------------------

## SAMPLE OUTPUT RESPONSES FROM SERVER  
=======================================
#  Sample of Server Response for Valid QR code:
                                              Server Response:
                                              SUCCESS (0): http://web.cs.wpi.edu/~cshue/cs3516/

# Sample of Server Response for Invalid QR code:
                                              Server response:
                                              FAILURE (1): Failed to decode QR Code or file error.

# Sample of Server Response for Rate Limit Exceeds:
                                             Server response:
                                             RATE_LIMIT (3): Rate limit exceeded. Please wait before sending another requests.

# Sample of Server Response for Time-out: 
                                             Server Response:
                                             TIMEOUT (2): Server timed out your connection.
-------------------------------------------------------------------------------------------------------------------------------
 
## LOGS
========
        The server creates administrative log reports file (i.e admin_log.txt) to record:
          - Connections and disconnections.
          - Rate-limit violations.
          - Errors (e.g., invalid file size or decoding issues).
          - Timeout events.
     
        => To view the logs: cat admin_log.txt
-------------------------------------------------------------------------------------------

## CLEANUP
===========
           To clean compiled binaries run: make clean 

===================================== *** The End *** ========================================


