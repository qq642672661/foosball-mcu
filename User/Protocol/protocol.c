#include "protocol.h"

extern int												SN;
extern uint8_t 										get_one_package;
extern pro_commonCmd							m_pro_commonCmd;
extern pro_errorCmd								m_pro_errorCmd;
extern m2w_returnMcuInfo					m_m2w_returnMcuInfo;
extern w2m_controlMcu							m_w2m_controlMcu;
extern m2w_mcuStatus							m_m2w_mcuStatus;
extern m2w_mcuStatus							m_m2w_mcuStatus_reported;
extern w2m_reportModuleStatus			m_w2m_reportModuleStatus;
extern uint8_t										check_status_time;
extern uint8_t										report_status_idle_time;	
extern uint8_t 										wait_ack_time;
extern uint8_t 										uart_buf[256];
extern uint16_t										uart_Count;
extern uint32_t										wait_wifi_status;
extern game											m_game;

/*******************************************************************************
* Function Name  : exchangeBytes
* Description    : 模拟的htons 或者 ntohs，如果系统支字节序更改可直接替换成系统函数
* Input          : value
* Output         : None
* Return         : 更改过字节序的short数值
* Attention		   : None
*******************************************************************************/
short	exchangeBytes(short	value)
{
	short			tmp_value;
	uint8_t		*index_1, *index_2;
	
	index_1 = (uint8_t *)&tmp_value;
	index_2 = (uint8_t *)&value;
	
	*index_1 = *(index_2+1);
	*(index_1+1) = *index_2;
	
	return tmp_value;
}

void updateLEDDisplay(void) {
	LED4_Display(m_m2w_mcuStatus.status_r.red_goals, m_m2w_mcuStatus.status_r.red_score, m_m2w_mcuStatus.status_r.blue_score, m_m2w_mcuStatus.status_r.blue_goals);
}

void updateGameStatus(void) {	
	m_m2w_mcuStatus.status_r.actions = m_game.last_goal << 1;
	m_m2w_mcuStatus.status_r.actions |= m_game.last_goal_member << 3;
}

void resetGame(uint8_t newGameId) {
	m_m2w_mcuStatus.status_w.game_id = newGameId;
	m_m2w_mcuStatus.status_r.blue_goals = 0;
	m_m2w_mcuStatus.status_r.blue_score = 0;
	m_m2w_mcuStatus.status_r.red_goals = 0;
	m_m2w_mcuStatus.status_r.red_score = 0;
	m_game.last_goal = 0;
	m_game.last_goal_member = 0;
	m_m2w_mcuStatus.status_w.game_control = GAME_STATUS_GOING;
	
	updateGameStatus();
}

void cancelBall(uint8_t cancelSide) {
	if (cancelSide == CANCEL_BLUE_GOAL) {
		if (m_m2w_mcuStatus.status_r.blue_goals == 0) return;
		if (m_m2w_mcuStatus.status_r.blue_goals == 10) {
			m_m2w_mcuStatus.status_r.blue_score -= 1;
		}
		m_m2w_mcuStatus.status_r.blue_goals -= 1;
	} else if (cancelSide == CANCEL_RED_GOAL) {
		if (m_m2w_mcuStatus.status_r.red_goals == 0) return;
		if (m_m2w_mcuStatus.status_r.red_goals == 10) {
			m_m2w_mcuStatus.status_r.red_score -= 1;
		}
		m_m2w_mcuStatus.status_r.red_goals -= 1;
	}
	m_game.last_goal = SIDE_UNKNOWN;
	m_game.last_goal_member = MEMBER_DEFAULT;
	
	updateGameStatus();
}

/*******************************************************************************
* Function Name  : SendCommonCmd
* Description    : 发送通用命令，命令字和sn作为参数传入，复用通用命令帧，写串口
* Input          : cmd：要发送的通用命令命令字， sn：序列号
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void	SendCommonCmd(uint8_t cmd, uint8_t sn)
{
	memset(&m_pro_commonCmd, 0, sizeof(pro_commonCmd));
	
	m_pro_commonCmd.head_part.head[0] = 0xFF;
	m_pro_commonCmd.head_part.head[1] = 0xFF;
	m_pro_commonCmd.head_part.len = exchangeBytes(5);
	m_pro_commonCmd.head_part.cmd = cmd;
	m_pro_commonCmd.head_part.sn = sn;
	m_pro_commonCmd.sum = CheckSum((uint8_t *)&m_pro_commonCmd, sizeof(pro_commonCmd));
	
	SendToUart((uint8_t *)&m_pro_commonCmd, sizeof(pro_commonCmd), 0);		
}

/*******************************************************************************
* Function Name  : SendErrorCmd
* Description    : 发送错误命令帧，错误码和sn作为参数传入，各种错误码适用
* Input          : error_no:错误码； sn:序列号
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void	SendErrorCmd(uint8_t error_no, uint8_t sn)
{
	m_pro_errorCmd.head_part.sn = sn;
	m_pro_errorCmd.error = error_no;
	m_pro_errorCmd.sum = CheckSum((uint8_t *)&m_pro_errorCmd, sizeof(pro_errorCmd));
	
	SendToUart((uint8_t *)&m_pro_errorCmd, sizeof(pro_errorCmd), 0);		
}

/*******************************************************************************
* Function Name  : CmdGetMcuInfo
* Description    : 返回mcu信息，仅更新sn即可，其他信息固化
* Input          : Sn：序号
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void	CmdGetMcuInfo(uint8_t sn)
{
	m_m2w_returnMcuInfo.head_part.sn = sn;
	m_m2w_returnMcuInfo.sum = CheckSum((uint8_t *)&m_m2w_returnMcuInfo, sizeof(m2w_returnMcuInfo));
	
	SendToUart((uint8_t *)&m_m2w_returnMcuInfo, sizeof(m2w_returnMcuInfo), 0);			
}

/*******************************************************************************
* Function Name  : CmdSendMcuP0
* Description    : mcu接收到wifi的控制命令，此部分是需要mcu开发者重点实现的
* Input          : buf：串口接收缓冲区地址
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void	CmdSendMcuP0(uint8_t *buf)
{
	uint8_t		tmp_cmd_buf;
	
	if(buf == NULL) return ;
	
	memcpy(&m_w2m_controlMcu, buf, sizeof(w2m_controlMcu));                                                                                                                                                                                            
	
	//上报状态
	if(m_w2m_controlMcu.sub_cmd == SUB_CMD_REQUIRE_STATUS) ReportStatus(REQUEST_STATUS);
	
	//控制命令，操作字段顺序依次是： game_control, game_id
	if(m_w2m_controlMcu.sub_cmd == SUB_CMD_CONTROL_MCU){
		//先回复确认，表示收到合法的控制命令了�
		SendCommonCmd(CMD_SEND_MCU_P0_ACK, m_w2m_controlMcu.head_part.sn);
		
		//控制命令标志按照协议表明哪个操作字段有效（对应的位为1）want to control game 
		if((m_w2m_controlMcu.cmd_tag & 0x01) == 0x01)
		{
			//0 ongoing, 1: start, 2: finish, 3: cancel last ball
			uint8_t control = m_w2m_controlMcu.status_w.game_control & 0x07;
			if(control == 0x01) // start 
			{
				//LED4_Display(0, 0x10, 0x10, 0);
				
						//控制 game id
				if((m_w2m_controlMcu.cmd_tag & 0x02) == 0x02)
				{
					resetGame(m_w2m_controlMcu.status_w.game_id);
				} else {
					resetGame(m_m2w_mcuStatus.status_w.game_id + 1);
				}
			} if (control == CANCEL_RED_GOAL || control == CANCEL_BLUE_GOAL) {
				cancelBall(control);
			} else                                    
			{
			}
			m_m2w_mcuStatus.status_w.game_control = GAME_STATUS_GOING;
		}

		
		ReportStatus(REPORT_STATUS);
	}
}

/*******************************************************************************
* Function Name  : CmdReportModuleStatus
* Description    : mcu收到来自于wifi模块的状态变化通知，此部分是mcu开发者重点实现的，比如在设备的面板上显示wifi的当前状态
* Input          : buf:串口接收缓冲区地址
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void	CmdReportModuleStatus(uint8_t *buf)
{
	if(buf == NULL)	return ;
	
	memcpy(&m_w2m_reportModuleStatus, buf, sizeof(w2m_reportModuleStatus));
	
	//0 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x01) == 0x01)
	{
		//打开了softap功能
	}
	else
	{
		//关闭了softap功能
	}
	
	//1 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x02) == 0x02)
	{
		//打开了station功能
	}
	else
	{
		//关闭了station功能
	}
	
	//2 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x04) == 0x04)
	{
		//开启了onboarding功能（AirLink）
	}
	else
	{
		//关闭了onboarding功能（AirLink）
	}
	
	//3 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x08) == 0x08)
	{
		//开启了binding功能
	}
	else
	{
		//关闭了binding功能
	}
	
	//4 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x10) == 0x10)
	{
		//连接路由器成功
		if(wait_wifi_status == 1){
			wait_wifi_status = 0;
			LED_RGB_Control(0, 0, 0);
			//LED4_Display(0, 0, 0, 8);
		}
	}
	else
	{
		//从路由器断开
		//LED4_Display(0, 0, 0, 0xE);
	}
	
	//5 bit
	if((m_w2m_reportModuleStatus.status[1] & 0x20) == 0x20)
	{
		//连接服务器成功
		if(wait_wifi_status == 1){
			wait_wifi_status = 0;
			LED_RGB_Control(0, 0, 0);
			//LED4_Display(0, 0, 8, 0);
		}
	}
	else
	{
		//从服务器断开
		//LED4_Display(0, 0, 0xE, 0);
	}
	
	SendCommonCmd(CMD_REPORT_MODULE_STATUS_ACK, m_w2m_reportModuleStatus.head_part.sn);
}
	
/*******************************************************************************
* Function Name  : MessageHandle
* Description    : 串口有数据发生了，先检查数据是否合法，再解析数据帧，做相应处理
* Input          : None
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void MessageHandle(void)
{
	pro_headPart	tmp_headPart;		
	memset(&tmp_headPart, 0, sizeof(pro_headPart));
	
	if(get_one_package)
	{			
		get_one_package = 0;
		memcpy(&tmp_headPart, uart_buf, sizeof(pro_headPart));
		
		//校验码错误，返回错误命令
		if(CheckSum(uart_buf, uart_Count) != uart_buf[uart_Count-1]) 
		{
			SendErrorCmd(ERROR_CHECKSUM, tmp_headPart.sn);
			return ;
		}
		
		switch(tmp_headPart.cmd)
		{
			//获取mcu infi，通用协议，mcu开发者仅更改必要的信息（比如product key）即可
			case	CMD_GET_MCU_INFO:
				CmdGetMcuInfo(tmp_headPart.sn);					
				break;	
			
			//心跳，mcu开发者复用即可，不需改变
			case CMD_SEND_HEARTBEAT:
				SendCommonCmd(CMD_SEND_HEARTBEAT_ACK, tmp_headPart.sn);
				break;
			
			//重启，mcu开发者复用即可，更改成系统的重启函数即可，必须等待600毫秒再重启
			case CMD_REBOOT_MCU:
				SendCommonCmd(CMD_REBOOT_MCU_ACK, tmp_headPart.sn);
				delay_ms(600);
				NVIC_SystemReset();
			
			//控制命令，mcu开发者套用模板，重点实现解析出来的命令正确操作外设
			case	CMD_SEND_MCU_P0:
				CmdSendMcuP0(uart_buf);
				break;
			
			//wifi状态变化通知，mcu开发者套用模板，重点实现操作外设
			case	CMD_REPORT_MODULE_STATUS:
				CmdReportModuleStatus(uart_buf);
				break;
			
			//发送错误命令字
			default:
				SendErrorCmd(ERROR_CMD, tmp_headPart.sn);
				break;
		}	
	}
	
}

/*******************************************************************************
* Function Name  : SendToUart
* Description    : 向串口发送数据帧
* Input          : buf:数据起始地址； packLen:数据长度； tag=0,不等待ACK；tag=1,等待ACK；
* Output         : None
* Return         : None
* Attention		   : 若等待ACK，按照协议失败重发3次；数据区出现FF，在其后增加55
*******************************************************************************/
void SendToUart(uint8_t *buf, uint16_t packLen, uint8_t tag)
{
	uint16_t 				i;
	int							Send_num;
	pro_headPart		send_headPart;	
	pro_commonCmd		recv_commonCmd;
	uint8_t					m_55;
	
	m_55 = 0x55;
	for(i=0;i<packLen;i++){
		UART1_Send_DATA(buf[i]);
		//当数据区出现FF时，追加发送55，包头的FF FF不能追加55
		if(i >=2 && buf[i] == 0xFF) UART1_Send_DATA(m_55);		
	}
	
	//如果tag＝0，不需要等待ACK；
	if(tag == 0) return ;
	
	memcpy(&send_headPart, buf, sizeof(pro_headPart));
	memset(&recv_commonCmd, 0, sizeof(pro_commonCmd));
	
	wait_ack_time = 0;
	Send_num = 1;
	
	while(Send_num < MAX_SEND_NUM)
	{
		if(wait_ack_time < MAX_SEND_TIME)
		{
			if(get_one_package)
			{				
				get_one_package = 0;
				memcpy(&recv_commonCmd, uart_buf, sizeof(pro_commonCmd));
				
				//只有当sn和ACK均配对时，才表明发送成功；
				if((send_headPart.cmd == CMD_SEND_MODULE_P0 && recv_commonCmd.head_part.cmd == CMD_SEND_MODULE_P0_ACK) &&
					(send_headPart.sn == recv_commonCmd.head_part.sn)) break;
				
				if((send_headPart.cmd == CMD_SET_MODULE_WORKMODE && recv_commonCmd.head_part.cmd == CMD_SET_MODULE_WORKMODE_ACK) &&
					(send_headPart.sn == recv_commonCmd.head_part.sn)) break;
				
				if((send_headPart.cmd == CMD_RESET_MODULE && recv_commonCmd.head_part.cmd == CMD_RESET_MODULE_ACK) &&
					(send_headPart.sn == recv_commonCmd.head_part.sn)) break;
			}
		}
		else 
		{
				wait_ack_time = 0 ;
			  for(i=0;i<packLen;i++){
					UART1_Send_DATA(buf[i]);
					if(i >=2 && buf[i] == 0xFF) UART1_Send_DATA(m_55);
				}
				Send_num ++;		
		}	
	}	
}

/*******************************************************************************
* Function Name  : CheckSum
* Description    : 校验和算法
* Input          : buf:数据起始地址； packLen:数据长度；
* Output         : None
* Return         : 校验码
* Attention		   : None
*******************************************************************************/
uint8_t CheckSum( uint8_t *buf, int packLen )
{
  int				i;
	uint8_t		sum;
	
	if(buf == NULL || packLen <= 0) return 0;

	sum = 0;
	for(i=2; i<packLen-1; i++) sum += buf[i];

	return sum;
}


/*******************************************************************************
* Function Name  : ReportStatus
* Description    : 上报mcu状态
* Input          : tag=REPORT_STATUS，主动上报，使用CMD_SEND_MODULE_P0命令字；
									 tag=REQUEST_STATUS，被动查询，使用CMD_SEND_MCU_P0_ACK命令字；
* Output         : None
* Return         : None
* Attention		   : None
*******************************************************************************/
void ReportStatus(uint8_t tag)
{
	if(tag == REPORT_STATUS)
	{
		m_m2w_mcuStatus.head_part.cmd = CMD_SEND_MODULE_P0;
		m_m2w_mcuStatus.head_part.sn = ++SN;
		m_m2w_mcuStatus.sub_cmd = SUB_CMD_REPORT_MCU_STATUS;
		//m_m2w_mcuStatus.status_w.motor_speed = exchangeBytes(m_m2w_mcuStatus.status_w.motor_speed);
		m_m2w_mcuStatus.sum = CheckSum((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
		SendToUart((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus), 1);
	}
	else if(tag == REQUEST_STATUS)
	{
		m_m2w_mcuStatus.head_part.cmd = CMD_SEND_MCU_P0_ACK;
		m_m2w_mcuStatus.head_part.sn = m_w2m_controlMcu.head_part.sn;
		m_m2w_mcuStatus.sub_cmd = SUB_CMD_REQUIRE_STATUS_ACK;
		//m_m2w_mcuStatus.status_w.motor_speed = exchangeBytes(m_m2w_mcuStatus.status_w.motor_speed);
		m_m2w_mcuStatus.sum = CheckSum((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
		SendToUart((uint8_t *)&m_m2w_mcuStatus, sizeof(m2w_mcuStatus), 0);
	}
		

	//m_m2w_mcuStatus.status_w.motor_speed = exchangeBytes(m_m2w_mcuStatus.status_w.motor_speed);
	memcpy(&m_m2w_mcuStatus_reported, &m_m2w_mcuStatus, sizeof(m2w_mcuStatus));
	
	
	
}


/*******************************************************************************
* Function Name  : CheckStatus
* Description    : 检查mcu状态，是否需要主动上报
* Input          : None
* Output         : None
* Return         : None
* Attention		   : 	1、最快2秒检查一次状态是否有变化；
										2、检查最新的mcu的状态与发送过的状态是否相同，不同就发送；
										3、若时间间隔大于10分钟，无论状态是否变化，都要上报一次状态；
*******************************************************************************/
void	CheckStatus(void)
{
	int					i, diff;
	uint8_t			*index_new, *index_old;
	
	diff = 0;
	//DHT11_Read_Data(&m_m2w_mcuStatus.status_r.temputure, &m_m2w_mcuStatus.status_r.humidity);
	updateLEDDisplay();
	if(check_status_time < 20) return ;
		
	check_status_time = 0;
	index_new = (uint8_t *)&(m_m2w_mcuStatus.status_w);
	index_old = (uint8_t *)&(m_m2w_mcuStatus_reported.status_w);
		
	for(i=0; i<sizeof(status_writable); i++)
	{
		if(*(index_new+i) != *(index_old+i)) diff += 1;
	}
		
	if(diff == 0)
	{
		index_new = (uint8_t *)&(m_m2w_mcuStatus.status_r);
		index_old = (uint8_t *)&(m_m2w_mcuStatus_reported.status_r);
			
		for(i=0; i<sizeof(status_readonly); i++)
		{
			if(*(index_new+i) != *(index_old+i)) diff += 1;
		}
	}
	
	//如果状态10分钟没有变化，强制上报一次；
	if(diff > 0 || report_status_idle_time > 60000) ReportStatus(REPORT_STATUS);
	if(report_status_idle_time > 60000) report_status_idle_time = 0;
	Motor_status(5);
	
}
