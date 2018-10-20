/** 
 *
 * @Author Sarah Alvarez (sarahal1@umbc.edu)
 * This file contains a program that simulates a food delivery service, using
 * multiple threads to represent drivers and restaurants. Condition variables and
 * a mutex is used for thread synchronization. 
 *
 **/


#define _XOPEN_SOURCE 500
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

pthread_mutex_t lock;
pthread_cond_t insertOrder;
pthread_cond_t takeOrder;
pthread_cond_t timed;

FILE* fp;                      // Open file.
char line[80];                 // Store line. No lines longer than 80 characters.
const char delimiter[2] = " "; // Split line on " ".


int T;                  // Max time to run (seconds). 
int D;                  // Number of drivers specified in file.
int count = 0;          // Indicated how many seconds have passed.
int finalOrderFlag = 0; // Indicates if the restaurant has processed the last order.
                        //    - 0 if no, 1 if yes.


/* Holds all information about a restaurant. */
typedef struct restaurant{
  int X;      
  int Y;
  int restID;
} rest_t;
rest_t restArray[10]; // Common place to hold all restaurants.

/* Holds all information about a customer */
typedef struct customer{
  int X;
  int Y;
  int custID;
} cust_t;
cust_t custArray[10]; // Common place to hold all customers.

/* Holds all information about an order */
typedef struct order{
  int orderID;
  int resNum;
  int custNum;
} order_t;
order_t* readyOrder = NULL; // Processed orders from restaurant stored here. 
                            // Drivers may also access this to take order.

/* Information about the restaurant thread */
typedef struct restThread{
  pthread_t pth;
  unsigned thread_id;
} restTh_t;

/* Information about the driver threads */
typedef struct driverThread{
  pthread_t pth;
  unsigned thread_id;
  int posX;
  int posY;
  int status;     // status may be 0 for idle or 1 for busy.
  int completed;  // Hold number of orders a specific driver has completed.
} driver_t;


/*** FUNCTIONS BEGIN HERE ***/

/**
 * restAction()
 * 
 * arg: is passed a struct of type restTh_t AKA a restThread struct.
 *
 * Parses through the rest of the file. Sleeps if the order is negative.
 * Puts order in readyOrder variable otherwise.
 * Only terminated itself if it has parsed all lines or if time runs out.
 *
 **/
static void* restAction(void* arg){
  int orderCount = 0;                   // Used to give orderIDs to orders.
  order_t temp;                         // Current parsed order.
  char* currLine = fgets(line, 80, fp); // Current line of file.

  /* Loop until EOF or the timer runs out. */
  while (currLine != NULL && count < T){
    pthread_mutex_lock(&lock);   // Get the lock.

    /* Loop only if the last order processed was not taken */
    /* and the timer is still running.                     */
    while (readyOrder != NULL && count < T){
      pthread_cond_wait(&takeOrder, &lock);  // Wait for a customer to take order.
    }

    /* Put up an order AKA Produce an item */
    /* Check if the timer has run out */
    if (count >= T){
      pthread_cond_signal(&timed);          // Signal main and restaurant threads
      pthread_cond_broadcast(&insertOrder); // one last time before terminating.
      pthread_mutex_unlock(&lock);          // Release the lock.
      break;
    }

    int tempRes = atoi(strtok(currLine, delimiter)); // Get the res, cust values from the line.
    int tempCust = atoi(strtok(NULL, delimiter));

    /* Sleep if negative, else put order in readyOrder for a driver.*/
    if (tempRes < 0 || tempCust < 0){
      usleep(250000);
    }
    else{
      temp.orderID = orderCount; // Give temp order the actual order values.
      temp.resNum = tempRes;
      temp.custNum = tempCust;
      
      readyOrder = &temp;     
      orderCount++;              // Increment for new orderID.
    }

    currLine = fgets(line, 80, fp); // Go to next line in file.

    /* If EOF and the timer is still going, indicate that last order was processed */
    if (currLine == NULL)
      finalOrderFlag = 1;
				   
    pthread_cond_signal(&timed);           // Signal main thread.
    pthread_cond_broadcast(&insertOrder);  // Signal all drivers.
    pthread_mutex_unlock(&lock);
  }

  /* Driver thread terminating */
  finalOrderFlag = 1;
  fclose(fp);
  return NULL;
}


/**
 * driverAction()
 * 
 * arg: is passed a struct of type driver_t AKA a driverThread struct
 *
 * Retrieves orders from readyOrder and carries out the order.
 *
 **/
static void* driverAction(void* arg){
  driver_t* temp;  // Current driver.

  /* Loop as long as the timer is still going */
  while (count < T){
    pthread_mutex_lock(&lock); // Obtain lock.
    temp = (driver_t*) arg;    // Set to current driver.

    /* Loop a long as there is no order (unless the las order was processed) */
    /* and the timer is still running.                                       */
    while (readyOrder == NULL && finalOrderFlag != 1 && count < T){
      temp->status = 0;                       // Set status to idle.
      pthread_cond_wait(&insertOrder, &lock); // Wait for an order.
    }
    
    /* Retrieve Order AKA Consume an item */
    /* Check if the timer has run out */
    if (count >= T){
      pthread_cond_signal(&timed);          // Signal the main, restaurant, and
      pthread_cond_signal(&takeOrder);      // other driver threads one last time
      pthread_cond_broadcast(&insertOrder); // before terminating itself.
      pthread_mutex_unlock(&lock);          // Release lock.
      break;
    }
    /* If the final order was processed, thread will idle. */
    else if(finalOrderFlag == 1){
      temp->status = 0;                   // Set status to idle.
      pthread_cond_signal(&timed);        // Signal everyone else so they
      pthread_cond_broadcast(&takeOrder); // can do stuff.
      pthread_mutex_unlock(&lock);
    }
    /* Get the order from readyOrder */
    else{
      order_t myOrder = *readyOrder;  // myOrder will be the thread's current order.
      readyOrder = NULL;              // Reset the readyOrder for the restaurant.
      temp->status = 1;               // Thread is now busy with the order.
      printf("  ** Restaurant: Assigning order %d (%d -> %d) to driver %d\n", myOrder.orderID, myOrder.resNum, myOrder.custNum, temp->thread_id);
      
      pthread_cond_signal(&timed);
      pthread_cond_broadcast(&takeOrder);
      pthread_mutex_unlock(&lock);
      
      /** Past Critical Section. Deliver Order **/
 
      /* Go to to Restaurant */
      int restX = restArray[myOrder.resNum].X;
      int restY = restArray[myOrder.resNum].Y;
      int manhattanDist = abs(restX - temp->posX) + abs(restY - temp->posY);
      int travelTime = manhattanDist * 250;
      printf("  * Driver %d: starting order %d, heading to restaurant %d (travel time = %d ms)\n", temp->thread_id, myOrder.orderID, myOrder.resNum, travelTime);
      usleep(travelTime * 1000);         // Multiply by 1000 for seconds -> milliseconds.
      temp->posX = restX;                // Update the driver's position.
      temp->posY = restY;
     
      /* Go to customer */
      int custX = custArray[myOrder.custNum].X;
      int custY = custArray[myOrder.custNum].Y;
      manhattanDist = abs(custX - temp->posX) + abs(custY - temp->posY);
      travelTime = manhattanDist * 250;
      temp->posX = custX; // Update the driver's position.
      temp->posY = custY;
      printf("  * Driver %d: picked up order %d, heading to customer %d (travel time = %d ms)\n", temp->thread_id, myOrder.orderID, myOrder.custNum, travelTime);
      usleep(travelTime * 1000);

      temp->status = 0;    // driver is now idle.
      (temp->completed)++; // Increments the amount of orders the driver has completed.
    }
  }
  
  return NULL;
}


/**
 * parseFile()
 * 
 * fileName: Name of the file to be parsed.
 *
 * Main thread. Parses the first 21 lines, then spawns driver and restaurant threads.
 * Keeps time and prints statuses of drivers.
 *
 **/
void parseFile(char* fileName){
  char* fgetsRet;            // Used to get rd of fgets warning.
  fp = fopen(fileName, "r"); // Open file.

  /* Error opening file */
  if (fp == NULL){
    printf("ERROR: Error opening file.\n");
    exit(0);
  }

  /* Get number of drivers */
  fgetsRet = fgets(line, 80, fp);
  D = atoi(line);

  /* Get restaurants */
  for (int i = 0; i < 10; i++){
    fgetsRet = fgets(line, 80, fp);
    rest_t newRest = {
      .X = atoi(strtok(line, delimiter)), // Splits up the line to get coordinates.
      .Y = atoi(strtok(NULL, delimiter)),
      .restID = i
    };
    restArray[i] = newRest; // Add the restaurant to the array.
    printf("Restaurant %d is at %d, %d\n", i, newRest.X, newRest.Y);
  }

  /* Get customers */
  for (int i = 0; i < 10; i++){
    fgetsRet = fgets(line, 80, fp);
    cust_t newCust = {
      .X = atoi(strtok(line, delimiter)), // Splits up the line to get coordinates.
      .Y = atoi(strtok(NULL, delimiter)),
      .custID = i
    };
    custArray[i] = newCust; // Add the customer to the customer array.
    printf("Customer %d is at %d, %d\n", i, newCust.X, newCust.Y);
  }

  if (fgetsRet){} // Get rid of return warning from fgets
    
  
  /* Make one thread for the restaurants */
  restTh_t restThread;
  pthread_create(&restThread.pth, NULL, restAction, &restThread);
  
  /* Make one thread per driver. */
  driver_t driverArray[D];
  for (int i = 0; i < D; i++){
    driverArray[i].thread_id = i;
    driverArray[i].posX = 0;
    driverArray[i].posY = 0;
    driverArray[i].status = 0;
    driverArray[i].completed = 0;
    pthread_create(&driverArray[i].pth, NULL, driverAction, &driverArray[i]);
  }

  /* Main thread keep time. */

  /* Loop untim max time. */
  while (count < T){
    printf("After %d seconds:\n", count);
    
    int total = 0;  // Holds the total amount of orders delivered.
    /* Print statuses of the drivers */
    for (int i = 0; i < D; i++){
      if (driverArray[i].status == 0)
	printf("  Driver %d: idle, completed %d orders\n", i, driverArray[i].completed);
      else if (driverArray[i].status == 1)
	printf("  Driver %d: busy, completed %d orders\n", i, driverArray[i].completed);
      total += driverArray[i].completed;
    }
    printf("Total completed deliveries: %d\n", total);
    fflush(stdout); // Force print.
    sleep(1);       // 1 second.

    /* CRITICAL SECTION** Update time */
    pthread_mutex_lock(&lock);
    pthread_cond_wait(&timed, &lock);
    count++;        // Increase amount of seconds passed.
    pthread_cond_signal(&insertOrder);
    pthread_cond_signal(&takeOrder);
    pthread_mutex_unlock(&lock);
  }
  
  /* Main thread wait for child threads */
  pthread_join(restThread.pth, NULL);
  for (int i = 0; i < D; i++){
    pthread_join(driverArray[i].pth, NULL);
  }
  
  /* Print final status of drivers */
  printf("*** FINAL STATUS After %d seconds:\n", count);
  int finalTotal = 0;
  for (int i = 0; i < D; i++){
    if (driverArray[i].status == 0)
      printf("  Driver %d: idle, completed %d orders\n", i, driverArray[i].completed);
    else if (driverArray[i].status == 1)
      printf("  Driver %d: busy, completed %d orders\n", i, driverArray[i].completed);
    finalTotal += driverArray[i].completed;
  }
  printf("Total completed deliveries: %d\n", finalTotal);
  
  
  /* Clean up locks and conds */
  pthread_mutex_destroy(&lock);
  pthread_cond_destroy(&timed);
  pthread_cond_destroy(&insertOrder);
  pthread_cond_destroy(&takeOrder);
  
}

int main(int argc, char* argv[]){
  /* If incorrect number of arguments, terminate */
  if (argc != 3){
    printf("ERROR: Invalid number of arguments.\n");
    exit(0);
  }
  
  T = atoi(argv[2]);  // Get max number of seconds.
  parseFile(argv[1]); // main thread's function.

  return 0;
}
