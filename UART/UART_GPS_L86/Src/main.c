#include "uart.h"
#include "string.h"
#include "stdio.h"
#include "stdlib.h"

char buf[10] = {0};

void MCO_pin_conf(){
	RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;
	GPIOA->CRH &= ~(GPIO_CRH_MODE8 | GPIO_CRH_CNF8);

	//CONFIGURE GPIO PIN MODE AS OUTPUT MAX SPEED 50MHZ
	GPIOA->CRH |= (GPIO_CRH_MODE8_1 | GPIO_CRH_MODE8_0);

	//CONFIGURE GPIO OUTPUT AS alternate function push pull
	GPIOA->CRH |= ((GPIO_CRH_CNF8_1) | ~(GPIO_CRH_CNF8_0));
}

void timer_init(){
	//Start by making sure that the timer's 'counter' is off
	TIM2->CR1 &= ~(TIM_CR1_CEN);
	TIM2->SR &= ~(TIM_SR_UIF);

	//RESET THE TIMER2 BUS
	RCC->APB1RSTR |= (RCC_APB1RSTR_TIM2RST);
	RCC->APB1RSTR &= ~(RCC_APB1RSTR_TIM2RST);

	//ENABLE THE TIMER2 PERIPHERAL CLOCK
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

	//ENABLE THE UPDATE GENERATION
	TIM2->EGR |= TIM_EGR_UG;

	/**********Eg1************/
	//Timer Prescaler value
	TIM2->PSC = 32;

	//Timer Auto Reload Register Value
	TIM2->ARR = 0xffff;

	TIM2->CR1 |=TIM_CR1_CEN;
//	while(!(TIM2->SR & (1<<TIM_SR_UIF)));
}

void system_clk()
{
	//ENABLE HSE AND WAIT TILL IT BECOMES READY
	RCC->CR |= RCC_CR_HSEON;	//HSE @8MHZ
	while(!(RCC->CR & RCC_CR_HSERDY))
		;

	//CONFIGURE FLASH PREFETCH AND LATENCY SETTINGS
	FLASH->ACR |= FLASH_ACR_PRFTBE|FLASH_ACR_LATENCY_1;

	//CONFIGURE PLL AND BUSES (AHB,APB1,APB2)
	//PLL SOURCE: HERE HSE IS USED AS A SOURCE
	RCC->CFGR |= RCC_CFGR_PLLSRC;
	//HSE DIVIDER FOR PLL (IF HSE IS USED AS SOURCE FOR PLL)
	RCC->CFGR |= RCC_CFGR_PLLXTPRE_HSE;	//INPUT TO PLL 8MHZ
	//PLL MULTIPLIER: HERE HSE OUTPUT IS MULTIPLIED WITH 4
	RCC->CFGR |= RCC_CFGR_PLLMULL4;	//OUTPUT TO PLL IS 32MHZ
	//BUS CLOCK CONFIGURE(APB1,APB2,AHB): NOT DIVIDING
	RCC->CFGR |= (RCC_CFGR_PPRE1_DIV1|RCC_CFGR_PPRE2_DIV1|RCC_CFGR_HPRE_DIV1);
	//ENABLE THE PLL
	RCC->CR |= RCC_CR_PLLON;
	//WAIT FOR PLL TO SET
	while(!(RCC->CR & RCC_CR_PLLRDY))
		;

	//ENABLE SYSTEMCLK AND WAIT
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while(!(RCC->CFGR & RCC_CFGR_SWS_PLL))
		;

	MCO_pin_conf();
	//CLOCK OUTPUT ON MCO PIN
	RCC->CFGR |= RCC_CFGR_MCO_SYSCLK;
}

void delay_us(uint16_t us){
	TIM2->CNT = 0;
	while(TIM2->CNT < us);
}

void delay_ms(uint16_t ms){
	for (uint16_t i=0; i<ms; i++)
		delay_us(1000);
}


//int main(void) {
//	system_clk();
//	timer_init();
//
//	uint8_t received_char;
//
//	// Initialize UART1 and UART2
//	uart1_init();
//	uart2_init();
//
//	while (1) {
//		// Process data from UART2 buffer
//		while (uart2_read_char(&received_char)) {
//			// Send each character to UART1
//			uart1_tran_byte(received_char);
//		}
//
//		// Perform other tasks if needed
//	}
//
//	return 0;
//	}

float nmeaToDecimal(float coordinate) {
    int degree = (int)(coordinate/100);
    float minutes = coordinate - degree * 100;
    float decimalDegree = minutes / 60;
    float decimal = degree + decimalDegree;
    return decimal;
}

// Function to calculate checksum by XOR-ing all characters between $ and *
uint8_t calculate_checksum(const char *nmea_sentence) {
    uint8_t checksum = 0;
    for (int i = 1; nmea_sentence[i] != '\r' && nmea_sentence[i] != '\n' && nmea_sentence[i] != '*' && nmea_sentence[i] != '\0' && i<75; i++) {
        checksum ^= nmea_sentence[i];  // XOR each character
    }
    return checksum;
}

// Function to extract the checksum from the sentence (after the *)
uint8_t extract_checksum(const char *nmea_sentence) {
    const char *checksum_ptr = strchr(nmea_sentence, '*'); // Find the '*' character
    if (checksum_ptr && *(checksum_ptr + 1) != '\0') {
        // Convert the two hexadecimal characters following '*' into a number
        return (uint8_t)strtol(checksum_ptr + 1, NULL, 16);
    }
    // Return 0 if '*' is not found or checksum is invalid
    return 0;
}


// Function to validate checksum
int validate_checksum(const char *nmea_sentence) {
    uint8_t calculated_checksum = calculate_checksum(nmea_sentence);
    uint8_t received_checksum = extract_checksum(nmea_sentence);

    // Check if calculated checksum matches the received checksum
//    char var[20];
//    sprintf(var, "I:%d C:%d", received_checksum, calculated_checksum);
//    uart1_tran_string(var);
//	uart1_tran_string();
    if (calculated_checksum == received_checksum) {
        return 1;  // Checksum is valid
    } else {
        return 0;  // Checksum is invalid
    }
}

// Function to handle time conversion from UTC to IST
void convert_utc_time_to_ist(float utcTime, int *istHour, int *istMinute, int *istSecond, int *dayIncrement) {
    // Extract time components (HH:MM:SS)
    int hour = (int)(utcTime / 10000);
    int minute = (int)((utcTime / 100)) % 100;
    int second = (int)(utcTime) % 100;

    // Add GMT offset (+5:30)
    int totalMinutes = minute + 30;
    int totalHours = hour + 5 + (totalMinutes / 60);
    totalMinutes %= 60;

    // Determine if a day increment is required (time wrap-around)
    *dayIncrement = 0;
    if (totalHours >= 24) {
        totalHours -= 24;  // Wrap around to the next day
        *dayIncrement = 1; // Signal day increment
    }

    // Set IST time components
    *istHour = totalHours;
    *istMinute = totalMinutes;
    *istSecond = second;
}

// Function to handle date conversion, accounting for day increment
void convert_utc_date_to_ist(float utcDate, int dayIncrement, int *istDay, int *istMonth, int *istYear) {
    // Extract date components (DD/MM/YY)
    int day = (int)(utcDate / 10000);
    int month = (int)((utcDate / 100)) % 100;
    int year = (int)(utcDate) % 100;

    // Apply day increment
    day += dayIncrement;

    // Handle month overflow (assuming 30 days for simplicity)
    if ((month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) && day > 31) {
        day = 1;
        month += 1;
    } else if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
        day = 1;
        month += 1;
    } else if (month == 2) {  // Handle February (no leap year check for simplicity)
        if (day > 28) {
            day = 1;
            month += 1;
        }
    }

    // Handle year overflow
    if (month > 12) {
        month = 1;
        year += 1;
    }

    // Set IST date components
    *istDay = day;
    *istMonth = month;
    *istYear = year+2000;
}


#define MAX_SATELLITES 12

struct data{
   	float lng;
   	float lat;
   	float utcTime;
   	char northsouth;
   	char eastwest;
   	char posStatus;
   	float dLng;
   	float dLat;
   	char fix;
   	int satCount;
   	float HDOP;
   	float MSLalt;
   	char altUnit;
   	float geoidSep;
   	char geoidSepUnit;
   	float ageDiffCorr;
   	float diffRefStatID;
   	char status;
   	char mode;
   	float tCourse;
   	char cRef;
   	float mCourse;
   	char mRef;
   	float speedN;
   	char speedUnitN;
   	float speedK;
	char speedUnitK;
   	char correctionMode;
   	float gndSpeed;
   	float gndHeading;
   	float utcDate;
   	float magDeclination;
   	char magDecDir;
   	char gnrmcMode;
    char navStatus;
    char gngsaMode;
    int satelliteIDs[MAX_SATELLITES];
	float PDOP;
	float VDOP;
	int posType;
	int gnssSysID;

   }gps;

   // Function to parse $GPVTG NMEA sentence
   int parse_gpvtg(const char *nmea_sentence, struct data *gps) {
       const char *delimiter = ",";
       char field[20];  // Temporary storage for each field
       const char *ptr = nmea_sentence;  // Pointer to iterate through the string
       int fieldIndex = 0;

       // Ensure the sentence starts with $GPVTG
       if (strncmp(nmea_sentence, "$GPVTG", 6) != 0) {
           return -1;  // Invalid sentence
       }

       ptr += 7;  // Skip "$GPVTG," prefix

       // Iterate through the fields
       while (*ptr) {
           const char *next = strstr(ptr, delimiter);

           // Extract the field
           if (next) {
               size_t len = next - ptr;
               strncpy(field, ptr, len);
               field[len] = '\0';  // Null-terminate
               ptr = next + 1;     // Move past the delimiter
           } else {
               strcpy(field, ptr);  // Last field
               ptr += strlen(ptr);  // Move to the end of the string
           }

           // Parse the field
           switch (fieldIndex) {
               case 0:  // True course
                   gps->tCourse = (strlen(field) > 0) ? atof(field) : 0.0;
                   break;
               case 1:  // True course reference
                   gps->cRef = (strlen(field) > 0) ? field[0] : '-';
                   break;
               case 2:  // Magnetic course
                   gps->mCourse = (strlen(field) > 0) ? atof(field) : 0.0;
                   break;
               case 3:  // Magnetic course reference
                   gps->mRef = (strlen(field) > 0) ? field[0] : '-';
                   break;
               case 4:  // Speed in knots
                   gps->speedN = (strlen(field) > 0) ? atof(field) : 0.0;
                   break;
               case 5:  // Speed unit for knots
                   gps->speedUnitN = (strlen(field) > 0) ? field[0] : '-';
                   break;
               case 6:  // Speed in km/h
                   gps->speedK = (strlen(field) > 0) ? atof(field) : 0.0;
                   break;
               case 7:  // Speed unit for km/h
                   gps->speedUnitK = (strlen(field) > 0) ? field[0] : '-';
                   break;
               case 8:  // Mode indicator
                   gps->correctionMode = (strlen(field) > 0) ? field[0] : '-';
                   break;
               default:
                   break;
           }

           fieldIndex++;
       }

       return 0;  // Parsing successful
   }

   int parse_gnrmc(const char *nmea_sentence, struct data *gps) {
       char sentence_copy[256];
       char *token;
       int field_count = 0;

       // Initialize fields in the structure
       gps->utcTime = 0.0f;
       gps->posStatus = '-';
       gps->lat = 0.0f;
       gps->northsouth = '-';
       gps->lng = 0.0f;
       gps->eastwest = '-';
       gps->gndSpeed = 0.0f;
       gps->gndHeading = 0.0f;
       gps->utcDate = 0.0f;
       gps->magDeclination = 0.0f;
       gps->magDecDir = '-';
       gps->gnrmcMode = '-';
       gps->navStatus = '-';

       // Copy the sentence to avoid modifying the original
       strncpy(sentence_copy, nmea_sentence, sizeof(sentence_copy));
       sentence_copy[sizeof(sentence_copy) - 1] = '\0';

       // Tokenize the sentence
       token = strtok(sentence_copy, ",");
       while (token != NULL) {
           field_count++;

           // Check which field we're on and process accordingly
           switch (field_count) {
               case 2: // UTC time
                   gps->utcTime = atof(token);
                   break;
               case 3: // Position Status
                   gps->posStatus = token[0];
                   break;
               case 4: // Latitude
                   gps->lat = atof(token);
                   break;
               case 5: // Latitude Direction
                   gps->northsouth = token[0];
                   break;
               case 6: // Longitude
                   gps->lng = atof(token);
                   break;
               case 7: // Longitude Direction
                   gps->eastwest = token[0];
                   break;
               case 8: // Ground Speed (Knots)
                   gps->gndSpeed = atof(token);
                   break;
               case 9: // Heading (Degrees)
                   gps->gndHeading = atof(token);
                   break;
               case 10: // UTC Date
                   gps->utcDate = atof(token);
                   break;
               case 11: // Magnetic Declination
                   if (*token != '\0') {
                       gps->magDeclination = atof(token);
                   }
                   break;
               case 12: // Magnetic Declination Direction
                   if (*token != '\0') {
                       gps->magDecDir = token[0];
                   }
                   break;
               case 13: // GN RMC Mode
                   if (*token != '\0') {
                       gps->gnrmcMode = token[0];
                   }
                   break;
               case 14: // Navigation Status
                   if (*token != '\0') {
                       gps->navStatus = token[0];
                   }
                   break;
               default:
                   break;
           }

           // Move to the next token
           token = strtok(NULL, ",");
       }

       // Debug Output
//       char debug[256];
//       snprintf(debug, sizeof(debug),
//                "{utcTime:%.6f, posStat:%c, Lat:%.6f,%c, Long:%.6f,%c, gndSpeed:%.6f, heading:%.6f, utcDate:%.6f, magDec:%.6f, magDecDir:%c, gnrmcMode:%c, navStat:%c}\r\n",
//                gps->utcTime, gps->posStat, gps->latitude, gps->latDir, gps->longitude, gps->longDir,
//                gps->gndSpeed, gps->heading, gps->utcDate, gps->magDec, gps->magDecDir,
//                gps->gnrmcMode, gps->navStat);
//       uart1_tran_string(debug);

       return field_count;
   }

   int parse_gngsa(const char *nmea_sentence, struct data *gngsa) {
       char sentence_copy[256];
       char *token;
       int field_count = 0;

       // Make a copy of the input sentence to tokenize
       strncpy(sentence_copy, nmea_sentence, sizeof(sentence_copy));
       sentence_copy[sizeof(sentence_copy) - 1] = '\0';

       // Tokenize using commas as delimiters
       token = strtok(sentence_copy, ",");

       // Initialize GNGSA structure
//       memset(gngsa, 0, sizeof(struct data));
//       gngsa->gngsaMode = '-';
//       gngsa->posType = 0;
//       gngsa->PDOP = 0.0f;
//       gngsa->HDOP = 0.0f;
//       gngsa->VDOP = 0.0f;

       int satellite_index = 0;

       // First pass: Count the total number of fields
       while (token != NULL) {
           field_count++;
           token = strtok(NULL, ",");
       }

       // Second pass: Parse the fields
       strncpy(sentence_copy, nmea_sentence, sizeof(sentence_copy));
       sentence_copy[sizeof(sentence_copy) - 1] = '\0';
       token = strtok(sentence_copy, ",");

       int current_field = 0;

       while (token != NULL) {
           current_field++;

           if (current_field == 2) {
               gngsa->gngsaMode = token[0];
           } else if (current_field == 3) {
               gngsa->posType = atoi(token);
           } else if (current_field >= 4 && current_field <= field_count - 4) {
               // Parse satellite IDs (fields 4 to field_count - 4)
               if (*token != '\0' && satellite_index < MAX_SATELLITES) {
                   gngsa->satelliteIDs[satellite_index++] = atoi(token);
               }
           } else if (current_field == field_count - 3) {
               // PDOP is the 4th-to-last field
               gngsa->PDOP = atof(token);
           } else if (current_field == field_count - 2) {
               // HDOP is the 3rd-to-last field
               gngsa->HDOP = atof(token);
           } else if (current_field == field_count - 1) {
               // VDOP is the 2nd last field
               gngsa->VDOP = atof(token);
           } else if (current_field == field_count) {
               // VDOP is the last field
               gngsa->gnssSysID = atof(token);
           }

           // Get the next token
           token = strtok(NULL, ",");
       }

       return field_count; // Return total fields parsed
   }

   int uart_receive_string(char *buffer, size_t max_length) {
       size_t index = 0;
       while (index < max_length - 1) {
           if (USART1->SR & USART_SR_RXNE) { // Check if data is received
               char received = USART1->DR;   // Read the received data
               if (received == '\n' || received == '\r') {
                   buffer[index] = '\0';    // Null-terminate the string
                   return 1;                // Indicate success
               } else {
                   buffer[index++] = received;
               }
           }
       }
       buffer[index] = '\0'; // Ensure null-termination
       return 0;             // No data received
   }


#define NMEA_MAX_LENGTH 300

char data[50];

char *nmea = "";

int main(void) {
	system_clk();
	timer_init();
    uint8_t received_char;
    char nmea_sentence[NMEA_MAX_LENGTH];  // Buffer for a single NMEA sentence
    uint16_t sentence_index = 0;          // Index to track buffer position

    // Initialize UART1 and UART2
    uart1_init();
    uart2_init();

    char debug[450];
	int istDay, istMonth, istYear;
	int istHour, istMinute, istSecond;
	int dayIncrement;


    while (1) {
        // Process data from UART2 buffer
	   if (uart_receive_string(buf, sizeof(buf))) {
		   if (strncmp(buf, "a",1)==0){
        while (uart2_read_char(&received_char)) {
            // Store the received character in the NMEA buffer
            if (sentence_index < (NMEA_MAX_LENGTH - 1)) {
                nmea_sentence[sentence_index++] = received_char;
            }

            // Check for end of an NMEA sentence
            if (received_char == '\n' && sentence_index > 2 && nmea_sentence[sentence_index - 2] == '\r') {
                nmea_sentence[sentence_index] = '\0';  // Null-terminate the string

                // Identify the sentence type
                if (!validate_checksum(nmea_sentence)) {
                    uart1_tran_string("Invalid checksum.\n");
                } else {
                    int parsed = 0;
//                    char debug[450];

                    if (strncmp(nmea_sentence + 1, "GPGGA", 5) == 0) {
//                	   uart1_tran_string("GPGGA sentence: ");
					   // Parse GPGGA sentence
					   parsed = sscanf(nmea_sentence,
									   "$GPGGA,%f,%f,%c,%f,%c,%c,%d,%f,%f,%c,%f,%c",
									   &gps.utcTime, &gps.lat, &gps.northsouth, &gps.lng, &gps.eastwest,
									   &gps.fix, &gps.satCount, &gps.HDOP, &gps.MSLalt, &gps.altUnit,
									   &gps.geoidSep, &gps.geoidSepUnit);

					   if (parsed >= 1) {
						   gps.dLat = nmeaToDecimal(gps.lat);
						   gps.dLng = nmeaToDecimal(gps.lng);

//                           convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);


					   } else {
//                           uart1_tran_string("Failed to parse GPGGA sentence.\n");
					   }
				   } else if (strncmp(nmea_sentence + 1, "GNRMC", 5) == 0) {
//                	   uart1_tran_string("GNRMC sentence: ");
					   // Parse GPGLL sentence
//                       parsed = sscanf(nmea_sentence,
//                                       "$GNRMC,%f,%c,%f,%c,%f,%c,%f,%f,%f,%f,%c,%c,%c",
//                                       &gps.utcTime, &gps.posStatus, &gps.dLat, &gps.northsouth,
//                                       &gps.dLng, &gps.eastwest, &gps.gndSpeed, &gps.gndHeading,
//									   &gps.utcDate, &gps.magDeclination, &gps.magDecDir, &gps.gnrmcMode, &gps.navStatus);
					   parse_gnrmc(nmea_sentence, &gps);
//                	   convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);

//                       if (parsed >= 1) {
//                    	   int istHour, istMinute, istSecond;
//                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);

//                           sprintf(debug, "{utcTime:%f, posStat:%c, Lat:%f,%c, Long:%f,%c, gndSpeed:%f, heading:%f, utcDate:%f, magDec:%f, magDecDir:%c, gnrmcMode:%c, navStat:%c}",
//                        		   gps.utcTime, gps.posStatus, gps.dLat, gps.northsouth,
//								   gps.dLng, gps.eastwest,gps.gndSpeed, gps.gndHeading,
//								   gps.utcDate, gps.magDeclination, gps.magDecDir, gps.gnrmcMode, gps.navStatus);
//                           uart1_tran_string(debug);
//                       } else {
//                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
//                       }

				   } else if (strncmp(nmea_sentence + 1, "GNGSA", 5) == 0) {
//                	   uart1_tran_string("GNGSA sentence: ");
					   // Parse GPGLL sentence
//                       parsed = sscanf(nmea_sentence,
//                                       "$GNGSA,%c,%d,%f,%c,%f,%c,%c",
//                                       &gps.gngsaMode, &gps.posType, &gps.dLng, &gps.eastwest,
//                                       &gps.utcTime, &gps.status, &gps.mode);
					   parse_gngsa(nmea_sentence, &gps);

//                       if (parsed >= 1) {
//                    	   int istHour, istMinute, istSecond;
//                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);

//                	   sprintf(debug, "{selectionMode:%c, fixType:%d, PDOP:%.2f, HDOP:%.2f, VDOP:%.2f, gnssSysID:%d, satelliteIDs:[",
//                	               gps.gngsaMode, gps.posType, gps.PDOP, gps.HDOP, gps.VDOP, gps.gnssSysID);
//                	       uart1_tran_string(debug);

						   for (int i = 0; i < MAX_SATELLITES; i++) {
							   if (gps.satelliteIDs[i] != 0) {  // Only print used satellites
								   gps.satCount++;
								   if (gps.satelliteIDs[i+1] != 0){
//									   sprintf(debug, "%d, ", gps.satelliteIDs[i]);
//									   uart1_tran_string(debug);
								   }else{
//                	        		   sprintf(debug, "%d", gps.satelliteIDs[i]);
//									   uart1_tran_string(debug);
								   }
							   }
						   }
//                	       uart1_tran_string("]}\r\n");
//                	       sprintf(debug, "\ntotal sats: %d\n", gps.satCount);
//                	       uart1_tran_string(debug);
//                       } else {
//                           uart1_tran_string("Failed to parse GNGSA sentence.\n");
//                       }
				   } else if (strncmp(nmea_sentence + 1, "GPGLL", 5) == 0) {
//                	   uart1_tran_string("GPGLL sentence: ");
					   // Parse GPGLL sentence
//                       parsed = sscanf(nmea_sentence,
//                                       "$GPGLL,%f,%c,%f,%c,%f,%c,%c",
//                                       &gps.lat, &gps.northsouth, &gps.lng, &gps.eastwest,
//                                       &gps.utcTime, &gps.status, &gps.mode);

					   if (parsed >= 1) {
//                    	   int istHour, istMinute, istSecond;
//                    	   convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);

//                           sprintf(debug, "{lat: %f%c, long:%f%c, utcTime:%f, istTime: %02d:%02d:%02d, gpsFix:%c, satCount:%d, HDOP:%f, MSLalt:%f, altUnit:%c, geoidSep:%f, geoidSepUnit:%c, ageDiffCorr:%f, diffID:%f}",
//                        		   gps.dLat, gps.northsouth, gps.dLng, gps.eastwest, gps.utcTime, istHour, istMinute, istSecond,
//								   gps.fix, gps.satCount, gps.HDOP, gps.MSLalt, gps.altUnit, gps.geoidSep, gps.geoidSepUnit, gps.ageDiffCorr, gps.diffRefStatID);
//                           uart1_tran_string(debug);
					   } else {
//                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
					   }
				   } else if (strncmp(nmea_sentence + 1, "GPVTG", 5) == 0) {
//                	   uart1_tran_string("GPVTG sentence: ");
					   // Parse GPGLL sentence
//                       parsed = sscanf(nmea_sentence,
//                                      "$GPVTG,%f,%c,%f,%c,%f,%c,%f,%c,%c",
//                                       &gps.tCourse, &gps.cRef, &gps.mCourse, &gps.mRef,
//                                       &gps.speedN, &gps.speedUnitN,&gps.speedK, &gps.speedUnitK, &gps.correctionMode);
					   parse_gpvtg(nmea_sentence, &gps);
//                       if (parsed >= 1) {
//                    	   int istHour, istMinute, istSecond;
//                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);

//                           sprintf(debug, "{tCourse:%f, cRef:%c, mCourse:%f, mRef:%c, speedN:%f, speedUnitN:%c, speedK:%f, speedUnitK:%c, correctionMode:%c}",
//                        		   gps.tCourse, gps.cRef, gps.mCourse, gps.mRef, gps.speedN, gps.speedUnitN, gps.speedK,
//								   gps.speedUnitK, gps.correctionMode);
//                           uart1_tran_string(debug);
//                       } else {
//                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
//                       }


				   } else {
//                       uart1_tran_string("Unsupported sentence type.\n");
				   }


                    // Transmit the NMEA sentence over UART1

			 //Convert time and determine if there's a day increment
				  convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);

			   // Convert date, applying the day increment if necessary
			      convert_utc_date_to_ist(gps.utcDate, dayIncrement, &istDay, &istMonth, &istYear);

//				  sprintf(debug, "{\"gDate\":\"%02d/%02d/%04d\", \"gTime\":\"%02d:%02d:%02d\", \"lat\":%f, \"long\":%f, \"gndSpeed\":%f, \"PDOP\":%.2f, \"HDOP\":%.2f, \"VDOP\":%.2f, \"MSLalt\":%f, \"altUnit\":\"%c\", \"speedN\":%f, \"speedUnitN\":\"%c\", \"speedK\":%f, \"speedUnitK\":\"%c\"}",
//				  istMonth, istDay, istYear ,istHour, istMinute, istSecond, gps.dLat, gps.dLng, gps.gndSpeed, gps.PDOP, gps.HDOP, gps.VDOP, gps.MSLalt, gps.altUnit, gps.speedN, gps.speedUnitN, gps.speedK, gps.speedUnitK);
				  sprintf(debug, "{\"gDate\":\"%02d/%02d/%04d\", \"gTime\":\"%02d:%02d:%02d\", \"lat\":%f, \"long\":%f, \"gndSpeed\":%f, \"PDOP\":%.2f, \"HDOP\":%.2f, \"VDOP\":%.2f, \"MSLalt\":%f, \"speedN\":%f, \"speedUnitN\":\"%c\", \"speedK\":%f, \"speedUnitK\":\"%c\"}",
				  istMonth, istDay, istYear ,istHour, istMinute, istSecond, gps.dLat, gps.dLng, gps.gndSpeed, gps.PDOP, gps.HDOP, gps.VDOP, gps.MSLalt, gps.speedN, gps.speedUnitN, gps.speedK, gps.speedUnitK);

		 //       sprintf(debug, "{\"lat\":%f, \"long\":%f, \"gtime\":%f, \"gdate\":%f}",
		 //       gps.dLat, gps.dLng, gps.utcTime,gps.utcDate);



				 //Print the result
			//	   sprintf(debug, "Resultant IST Date: %02d/%02d/%02d\n", istDay, istMonth, istYear);
			//	   uart1_tran_string(debug);
			//	   sprintf(debug, "Resultant IST Time: %02d:%02d:%02d\n", istHour, istMinute, istSecond);
				  delay_ms(10);
				  uart1_tran_string(debug);
				  delay_ms(10);
				   delay_ms(1000);

                    uart1_tran_string("\n\n");
                }

                // Reset the buffer for the next sentence
                sentence_index = 0;

            }
        }

    }
}

    }

}
//    char debug[450];
//    int istDay, istMonth, istYear;
//    int istHour, istMinute, istSecond;
//    int dayIncrement;
//    char buf[10] = {0};
//   while(1){
//	   if (uart_receive_string(buf, sizeof(buf))) {
//		   if (strncmp(buf, "a",1)==0){
//    // Simulate processing NMEA data
//       for (size_t i = 0; nmea[i] != '\0'; i++) {
//           char received_char = nmea[i];
//
//           // Store the received character in the buffer
//           if (sentence_index < (NMEA_MAX_LENGTH - 1)) {
//               nmea_sentence[sentence_index++] = received_char;
//           }
//
//           // Check for end of an NMEA sentence
//           if (received_char == '\n' && sentence_index > 2 && nmea_sentence[sentence_index - 2] == '\r') {
//               nmea_sentence[sentence_index] = '\0'; // Null-terminate the string
//
//               // Validate checksum
//               if (!validate_checksum(nmea_sentence)) {
//                   uart1_tran_string("Invalid checksum.\n");
//               } else {
//                   int parsed = 0;
//
//                   if (strncmp(nmea_sentence + 1, "GPGGA", 5) == 0) {
////                	   uart1_tran_string("GPGGA sentence: ");
//                       // Parse GPGGA sentence
//                       parsed = sscanf(nmea_sentence,
//                                       "$GPGGA,%f,%f,%c,%f,%c,%c,%d,%f,%f,%c,%f,%c",
//                                       &gps.utcTime, &gps.lat, &gps.northsouth, &gps.lng, &gps.eastwest,
//                                       &gps.fix, &gps.satCount, &gps.HDOP, &gps.MSLalt, &gps.altUnit,
//                                       &gps.geoidSep, &gps.geoidSepUnit);
//
//                       if (parsed >= 1) {
//                           gps.dLat = nmeaToDecimal(gps.lat);
//                           gps.dLng = nmeaToDecimal(gps.lng);
//
////                           convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);
//
//
//                       } else {
////                           uart1_tran_string("Failed to parse GPGGA sentence.\n");
//                       }
//                   } else if (strncmp(nmea_sentence + 1, "GNRMC", 5) == 0) {
////                	   uart1_tran_string("GNRMC sentence: ");
//                       // Parse GPGLL sentence
////                       parsed = sscanf(nmea_sentence,
////                                       "$GNRMC,%f,%c,%f,%c,%f,%c,%f,%f,%f,%f,%c,%c,%c",
////                                       &gps.utcTime, &gps.posStatus, &gps.dLat, &gps.northsouth,
////                                       &gps.dLng, &gps.eastwest, &gps.gndSpeed, &gps.gndHeading,
////									   &gps.utcDate, &gps.magDeclination, &gps.magDecDir, &gps.gnrmcMode, &gps.navStatus);
//                	   parse_gnrmc(nmea_sentence, &gps);
////                	   convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);
//
////                       if (parsed >= 1) {
////                    	   int istHour, istMinute, istSecond;
////                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);
//
////                           sprintf(debug, "{utcTime:%f, posStat:%c, Lat:%f,%c, Long:%f,%c, gndSpeed:%f, heading:%f, utcDate:%f, magDec:%f, magDecDir:%c, gnrmcMode:%c, navStat:%c}",
////                        		   gps.utcTime, gps.posStatus, gps.dLat, gps.northsouth,
////								   gps.dLng, gps.eastwest,gps.gndSpeed, gps.gndHeading,
////								   gps.utcDate, gps.magDeclination, gps.magDecDir, gps.gnrmcMode, gps.navStatus);
////                           uart1_tran_string(debug);
////                       } else {
////                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
////                       }
//
//                   } else if (strncmp(nmea_sentence + 1, "GNGSA", 5) == 0) {
////                	   uart1_tran_string("GNGSA sentence: ");
//                       // Parse GPGLL sentence
////                       parsed = sscanf(nmea_sentence,
////                                       "$GNGSA,%c,%d,%f,%c,%f,%c,%c",
////                                       &gps.gngsaMode, &gps.posType, &gps.dLng, &gps.eastwest,
////                                       &gps.utcTime, &gps.status, &gps.mode);
//                	   parse_gngsa(nmea_sentence, &gps);
//
////                       if (parsed >= 1) {
////                    	   int istHour, istMinute, istSecond;
////                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);
//
////                	   sprintf(debug, "{selectionMode:%c, fixType:%d, PDOP:%.2f, HDOP:%.2f, VDOP:%.2f, gnssSysID:%d, satelliteIDs:[",
////                	               gps.gngsaMode, gps.posType, gps.PDOP, gps.HDOP, gps.VDOP, gps.gnssSysID);
////                	       uart1_tran_string(debug);
//
//                	       for (int i = 0; i < MAX_SATELLITES; i++) {
//                	           if (gps.satelliteIDs[i] != 0) {  // Only print used satellites
//                	        	   gps.satCount++;
//                	        	   if (gps.satelliteIDs[i+1] != 0){
////									   sprintf(debug, "%d, ", gps.satelliteIDs[i]);
////									   uart1_tran_string(debug);
//                	        	   }else{
////                	        		   sprintf(debug, "%d", gps.satelliteIDs[i]);
////									   uart1_tran_string(debug);
//								   }
//                	           }
//                	       }
////                	       uart1_tran_string("]}\r\n");
////                	       sprintf(debug, "\ntotal sats: %d\n", gps.satCount);
////                	       uart1_tran_string(debug);
////                       } else {
////                           uart1_tran_string("Failed to parse GNGSA sentence.\n");
////                       }
//                   } else if (strncmp(nmea_sentence + 1, "GPGLL", 5) == 0) {
////                	   uart1_tran_string("GPGLL sentence: ");
//                       // Parse GPGLL sentence
////                       parsed = sscanf(nmea_sentence,
////                                       "$GPGLL,%f,%c,%f,%c,%f,%c,%c",
////                                       &gps.lat, &gps.northsouth, &gps.lng, &gps.eastwest,
////                                       &gps.utcTime, &gps.status, &gps.mode);
//
//                       if (parsed >= 1) {
////                    	   int istHour, istMinute, istSecond;
////                    	   convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);
//
////                           sprintf(debug, "{lat: %f%c, long:%f%c, utcTime:%f, istTime: %02d:%02d:%02d, gpsFix:%c, satCount:%d, HDOP:%f, MSLalt:%f, altUnit:%c, geoidSep:%f, geoidSepUnit:%c, ageDiffCorr:%f, diffID:%f}",
////                        		   gps.dLat, gps.northsouth, gps.dLng, gps.eastwest, gps.utcTime, istHour, istMinute, istSecond,
////								   gps.fix, gps.satCount, gps.HDOP, gps.MSLalt, gps.altUnit, gps.geoidSep, gps.geoidSepUnit, gps.ageDiffCorr, gps.diffRefStatID);
////                           uart1_tran_string(debug);
//                       } else {
////                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
//                       }
//                   } else if (strncmp(nmea_sentence + 1, "GPVTG", 5) == 0) {
////                	   uart1_tran_string("GPVTG sentence: ");
//                       // Parse GPGLL sentence
////                       parsed = sscanf(nmea_sentence,
////                                      "$GPVTG,%f,%c,%f,%c,%f,%c,%f,%c,%c",
////                                       &gps.tCourse, &gps.cRef, &gps.mCourse, &gps.mRef,
////                                       &gps.speedN, &gps.speedUnitN,&gps.speedK, &gps.speedUnitK, &gps.correctionMode);
//                	   parse_gpvtg(nmea_sentence, &gps);
////                       if (parsed >= 1) {
////                    	   int istHour, istMinute, istSecond;
////                    	   convert_utc_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond);
//
////                           sprintf(debug, "{tCourse:%f, cRef:%c, mCourse:%f, mRef:%c, speedN:%f, speedUnitN:%c, speedK:%f, speedUnitK:%c, correctionMode:%c}",
////                        		   gps.tCourse, gps.cRef, gps.mCourse, gps.mRef, gps.speedN, gps.speedUnitN, gps.speedK,
////								   gps.speedUnitK, gps.correctionMode);
////                           uart1_tran_string(debug);
////                       } else {
////                           uart1_tran_string("Failed to parse GPGLL sentence.\n");
////                       }
//
//
//                   } else {
////                       uart1_tran_string("Unsupported sentence type.\n");
//                   }
//
//
//               }
//
//               // Reset the buffer for the next sentence
//               sentence_index = 0;
//           }
//       }
//
//    	              // Print received data.
////    	              uart1_tran_string("Received: ");
////    	              uart1_tran_string(buf);
////    	              sprintf(debug, "%d", strncmp(buf, "a",1));
////    	              uart1_tran_string(debug);
////    	              uart1_tran_string("\n");
//
//    	              // Process the received input
////    	              process_input(buf);
//
//
//    	              // Clear the buffer after processing
//    	              memset(buf, 0, sizeof(buf));
//    	          }
//    	   }
//
//    	          // Add a delay to avoid flooding
////    	          delay_ms(1000);
//
//	   // Convert time and determine if there's a day increment
//	   convert_utc_time_to_ist(gps.utcTime, &istHour, &istMinute, &istSecond, &dayIncrement);
//
//	   // Convert date, applying the day increment if necessary
//	   convert_utc_date_to_ist(gps.utcDate, dayIncrement, &istDay, &istMonth, &istYear);
//
//       sprintf(debug, "{\"gDate\":\"%02d/%02d/%04d\", \"gTime\":\"%02d:%02d:%02d\", \"lat\":%f, \"long\":%f, \"gndSpeed\":%f, \"PDOP\":%.2f, \"HDOP\":%.2f, \"VDOP\":%.2f, \"MSLalt\":%f, \"altUnit\":\"%c\", \"speedN\":%f, \"speedUnitN\":\"%c\", \"speedK\":%f, \"speedUnitK\":\"%c\"}",
//       istMonth, istDay, istYear ,istHour, istMinute, istSecond, gps.dLat, gps.dLng, gps.gndSpeed, gps.PDOP, gps.HDOP, gps.VDOP, gps.MSLalt, gps.altUnit, gps.speedN, gps.speedUnitN, gps.speedK, gps.speedUnitK);
////       sprintf(debug, "{\"lat\":%f, \"long\":%f, \"gtime\":%f, \"gdate\":%f}",
////       gps.dLat, gps.dLng, gps.utcTime,gps.utcDate);
//
//
//
//
//	   // Print the result
////	   sprintf(debug, "Resultant IST Date: %02d/%02d/%02d\n", istDay, istMonth, istYear);
////	   uart1_tran_string(debug);
////	   sprintf(debug, "Resultant IST Time: %02d:%02d:%02d\n", istHour, istMinute, istSecond);
//	   uart1_tran_string(debug);
//       delay_ms(1000);
//       }
//
//       return 0;
//   }
