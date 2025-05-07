#include <stdio.h>
#include "stm32f4xx.h"
#include "fpu.h"
#include "uart.h"
#include "timebase.h"
#include "bsp.h"
#include "adc.h"
#include "w5500_driver.h"
#include "webpages.h"

#define RECEIVE_BUFF_SIZE	64
#define SOCKET_TO_USE		1
#define LISTEN_PORT			40


uint8_t receive_buff[RECEIVE_BUFF_SIZE];

wiz_PhyConf current_phy_conf;


void check_cable_connection(void)
{
	uint8_t cable_status;

	do{

		printf("\r\nGetting cable status...\r\n");
		ctlwizchip(CW_GET_PHYLINK,(void *)&cable_status);

		if(cable_status ==  PHY_LINK_OFF)
		{
			printf("No cable detected ...\r\n");
			delay(1000);
		}

	}while(cable_status ==  PHY_LINK_OFF);

	printf("Cable connected...\r\n");

}

void display_phy_config(void)
{
	wiz_PhyConf phy_conf;
	ctlwizchip(CW_GET_PHYCONF,(void*)&phy_conf);

	if(phy_conf.by == PHY_CONFBY_HW)
	{
		printf("\n\rPHY is currently configured by hardware.");
	}
	else{
		printf("\n\rPHY is currently configured by software.");

	}
	printf("\r\nSTATUS: Autonegotiation %s",(phy_conf.mode == PHY_MODE_AUTONEGO) ? "Enabled" : "Disabled");
	printf("\r\nSTATUS: Duplex Mode: %s",(phy_conf.duplex == PHY_DUPLEX_FULL) ? "Full Duplex" : "Half Duplex");
	printf("\r\nSTATUS: Speed: %dMbps",(phy_conf.speed == PHY_SPEED_10) ? 10 : 100);
  printf("\r\n...");

}


void handle_error(const char * message)
{
	printf("ERROR: %s\r\n",message);

	//Optionally reset the system

	while(1){
		//You could blink an LED here
	}
}


void send_sensor_data(uint32_t sensor_value, uint8_t socket_id)
{
	char sensor_str[14];
	sprintf(sensor_str,"%u\r\n",sensor_value);

	//send the sensor data string to the server/cleint
	if(send(socket_id,(uint8_t *)sensor_str, strlen(sensor_str)) <= 0)
	{
		printf("Failed to send sensor data.\r\n");
	}
	else
	{
		printf("Sensor data sent successfully: %s\r\n",sensor_str);

	}
}
int initialize_server_socket(void)
{
	printf("\r\nInitializing server socket....\r\n");


	   if(socket(SOCKET_TO_USE, Sn_MR_TCP,LISTEN_PORT,0) != SOCKET_TO_USE)
	   {
		   printf("\r\nFailed to create socket....");
		   	return -1;
	   }

	   uint8_t socket_io_mode = SOCK_IO_BLOCK;
	   ctlsocket(SOCKET_TO_USE,CS_SET_IOMODE,&socket_io_mode);

	   if(listen(SOCKET_TO_USE) != SOCK_OK)
	   {
		    handle_error("Failed to listen to socket...");
		    return -1;
	   }

	   printf("Socket initialized successfully, listening on port %d... \r\n",LISTEN_PORT);

	   return 0;

}

void check_for_exit_command(const char* data)
{
	if(strcmp(data, "EXIT") == 0)
	{
		printf("Received EXIT command. Disconnecting client...\r\n");
		//Do something... e.g. save data to sd card etc
		disconnect(SOCKET_TO_USE);

	}
}


void check_for_sys_on_command(const char* data)
{
	if(strcmp(data, "SYS ON") == 0)
	{
		printf("Turning system ON!!\r\n");
		//Do something... e.g. turn on LED, motor etc

	}
}

void check_for_sys_off_command(const char* data)
{
	if(strcmp(data, "SYS OFF") == 0)
	{
		printf("Turning system OFF!!\r\n");
		//Do something... e.g. turn off LED, motor etc

	}
}



int handle_client_data(void)
{
	int32_t received_len =  recv(SOCKET_TO_USE,receive_buff,RECEIVE_BUFF_SIZE);

	if(received_len == SOCKERR_SOCKSTATUS)
	{
		printf("Client has disconnected. Ending session...\r\n");
		return -1;
	}

	receive_buff[received_len] = '\0';
	printf("Received %d bytes from client: %s\r\n",received_len,receive_buff);

	return received_len;

}

void send_data_to_client(const char * data)
{
	int data_len =  strlen(data);
	if( send(SOCKET_TO_USE,(uint8_t *)data,data_len) != data_len)
	{
		handle_error("Failed to send data to the client.\r\n");
	}
	else
	{
		printf("Data sent to client: %s\r\n",data);
	}
}

void echo_back_to_client(const uint8_t * data, int length)
{
	send(SOCKET_TO_USE,(uint8_t *)"**",2);
	send(SOCKET_TO_USE,data,length);
	send(SOCKET_TO_USE,(uint8_t *)"**",2);
	printf("Echoe data back to client.\r\n");
}
int main()
{
	/*Enable FPU*/
	fpu_enable();

	/*Initialize timebase*/
	timebase_init();

	/*Initialize debug UART*/
	debug_uart_init();



	/*Initialize LED*/
	led_init();

	/*Initialize Push button*/
	button_init();

	/*Initialize ADC*/
	pa1_adc_init();

	/*Start conversion*/
	start_conversion();

    wizchip_cs_pin_init();
    w5500_spi_init();
    w5500_init();


    current_phy_conf.by =  PHY_CONFBY_SW;
    current_phy_conf.duplex =  PHY_DUPLEX_FULL;
    current_phy_conf.mode   =  PHY_MODE_AUTONEGO;
    current_phy_conf.speed =  PHY_SPEED_10;

    ctlwizchip(CW_SET_PHYCONF,(void*)&current_phy_conf);

    check_cable_connection();

    display_phy_config();

    printf("\r\n**********************TCP Server***********************\r\n");


	while(1)
	{
       if(initialize_server_socket() != 0)
       {
    	   continue;
       }
       printf("Waiting for a client connection...\r\n");

       uint8_t socket_status  = 0x00;

       do{
    	   socket_status = getSn_SR(SOCKET_TO_USE);
       }while(socket_status != SOCK_ESTABLISHED );

       if(socket_status == SOCK_ESTABLISHED)
       {

    	   while(1){
    	   int received_len = handle_client_data();

			   if(received_len == -1)
			   {
				   break; //client disconnected
			   }

			   send_data_to_client("Data from the server..!\r\n");
			   int sensor_value = adc_read();
			   send_sensor_data(sensor_value,SOCKET_TO_USE);

			   //Check for commands
			   check_for_exit_command((char *)receive_buff);
			   check_for_sys_on_command((char *)receive_buff);
			   check_for_sys_off_command((char *)receive_buff);

			   echo_back_to_client(receive_buff,received_len);

    	   }
       }
	}
}

