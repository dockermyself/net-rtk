//
// Created by dockermyself on 2022/4/19.
//

struct PARAM *init_param(struct CMD_MSG *msg, struct sockaddr_in *to_addr) {
    struct PARAM *param = malloc(sizeof(struct PARAM) + msg->data_len);
    memcpy(&param->to_addr, to_addr, sizeof(struct sockaddr_in));
    memcpy(&param->msg, msg, sizeof(struct CMD_MSG) + msg->data_len);
    return param;
}

void destroy_param(struct PARAM **param) {
    if (param == NULL || *param == NULL) {
        return;
    }
    free(*param);
    *param = NULL;
}



void init_buffer(struct EVENT_BUFFER *event, int buffer_size) {
    event->buffer = malloc(buffer_size);
    memset(event->buffer, 0, buffer_size);
    event->buffer_size = buffer_size;
    event->data_len = 0;
}

void *RECV_FROM_SERIAL(void *args) {
    struct EVENT_BUFFER nema;
    init_buffer(&nema, 128);
    struct EVENT_BUFFER param;
    init_buffer(&param, 1024);

    char head_code[10] = {0};
    const int CHECK_CODE_LEN = 2;
    int buffer_start = 0, buffer_end = 0;//设置start，end指针读取串口

    //如果第一包数据为RMC,那么等待HDT报文

    while (get_status() != STATUS_EXIT) {
        int n_serial = serial_read(SERIAL_DEV0, param.buffer + buffer_end, param.buffer_size / 2);
        //int n_serial = GGARead(param.buffer + buffer_end, para
        // m->buffer_size / 2);
        if (n_serial <= 0) {
            continue;
        }

        buffer_end += n_serial;//有效数据[buffer_start,buffer_end)
        buffer_start = nema_find_head(param.buffer, buffer_start, buffer_end, head_code, 10);


        int package_end;
        //有效数据[buffer_start,package_end]
        while ((package_end = nema_find_tail(param.buffer, buffer_start, buffer_end)) < buffer_end)//一次处理多包数据
        {

            //param.buffer[start] == '$',param.buffer[package_end] == ‘*’,查找[start,package_end)满足校验的'$'
            if (!nema_bbc_check_match(param.buffer, &buffer_start, package_end, head_code, 10)) {//没有查找到
                buffer_start = nema_find_head(param.buffer, package_end + 1, buffer_end, head_code, 10);
                continue;
            }

            memset(nema.buffer, 0, nema.buffer_size);
            nema_format_data(param.buffer, buffer_start, package_end + CHECK_CODE_LEN + 1,
                             nema.buffer, &nema.data_len);
            //%GNGGA发送给基准站
            if (strcmp(head_code, "$GPGGA") == 0 || strcmp(head_code, "$GNGGA") == 0) {
                if (nema_parse_gga(nema.buffer, NULL)) {
                    logger_write_message(NEMA_LOGGER_FILE, nema.buffer);
                    msg_cors_update(nema.buffer, nema.data_len);
                }


            } else if (strcmp(head_code, "$GNRMC") == 0 || strcmp(head_code, "$GPRMC") == 0) {
                struct Longitude longitude;
                struct Latitude latitude;
                double speed;
                if (nema_parse_rmc(nema.buffer, &latitude, &longitude, &speed)) {//解析经度，纬度信息
                    logger_write_message(NEMA_LOGGER_FILE, nema.buffer);
                    msg_rtk_update(&longitude, &latitude, &speed, NULL);
                }
            } else if (strcmp(head_code, "$GNHDT") == 0 || strcmp(head_code, "$GPHDT") == 0)//先获得$GNRMC，再获得$GNHDT
            {
                double heading = 0.0;
                if (nema_parse_hdt(nema.buffer, &heading)) {
                    logger_write_message(NEMA_LOGGER_FILE, nema.buffer);
                    msg_rtk_update(NULL, NULL, NULL, &heading);
                }

            }
            //更新start,准备解析下一包数据
            buffer_start = nema_find_head(param.buffer, package_end + CHECK_CODE_LEN + 1,
                                          buffer_end, head_code, 10);
        }
        if (buffer_start >= buffer_end) {
            buffer_start = 0, buffer_end = 0;
        } else if (buffer_start >= param.buffer_size / 2) {
            memcpy(param.buffer, param.buffer + buffer_start, buffer_end - buffer_start);
            buffer_end = buffer_end - buffer_start;
            buffer_start = 0;
        } else {
            //printf("data not complete,continue to read\n");
        }
    }

    release_buffer(&nema);
    release_buffer(&param);
    logger_write_message(MESSAGE_LOGGER_FILE, "task recv from serial exit");
    logger_warn("task recv from serial exit\n");
    return NULL;
}

void CORS_COMMUNICATE() {
    struct EVENT_BUFFER param;
    init_buffer(&param, 1024);
    uint8_t gga_data[256] = {0};
    int time_out = 0;
    const int TIME_OUT_MAX = 50;//超时时间
    const int SLEEP_TIME = 200 * 1000;//休眠时间
    while (get_status() != STATUS_EXIT) {
        int recv_n = cors_recv(param.buffer, param.buffer_size);
        //print_bytes((const uint8_t *) param.buffer, param.buffer_size);
        if (recv_n < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (++time_out > TIME_OUT_MAX) {
                logger_write_message(MESSAGE_LOGGER_FILE, "cors recv timeout");
                logger_warn("cors recv timeout\n");
                break;
            }

        } else if (recv_n == 0) {//接收到FIN位，服务器断开连接
            logger_warn("cors disconnected\n");
            logger_write_message(MESSAGE_LOGGER_FILE, "cors disconnected");
            break;
        } else if (recv_n > 0) {
            //发送数据
            logger_write_bytes(RTCM_LOGGER_FILE, (uint8_t *) param.buffer, recv_n);
            if (serial_write(SERIAL_DEV0, param.buffer, recv_n) < 0) {
                break;
            }
            time_out = 0;
        }
        memset(gga_data, 0, sizeof(gga_data));
        if (msg_cors_get((char *) gga_data) <= 0) {
            usleep(SLEEP_TIME);
            continue;
        }
        int send_n = cors_send((char *) gga_data, (int) strlen((const char *) gga_data) + 1);
        if (send_n == 0) {
            logger_warn("cors disconnected\n");
            logger_write_message(MESSAGE_LOGGER_FILE, "cors disconnected");
            break;
        }
        usleep(SLEEP_TIME);
    }
    set_connect_status(cors_disconnected);
    cors_close();
    release_buffer(&param);
    logger_write_message(MESSAGE_LOGGER_FILE, "cors communicate end");
    logger_warn("cors communicate end\n");

}


//登录CORS（Continuously Operating Reference Stations）就是网络基准站
void *LOGIN_CORS(void *args) {

    struct PARAM *param = (struct PARAM *) args;
    struct USER_LOGIN *login = (struct USER_LOGIN *) param->msg.data;
    cors_client_t *client = cors_client_init(login->ip, login->port, login->mount_point,
                                             login->user, login->password);
    logger_write_message(MESSAGE_LOGGER_FILE, "task login cors start");
    while (true) {//loop for connect server

        if (cors_connect(client) < 0) {
            cmd_send_ctl_status(&param->msg, &param->to_addr, 0x00);
            break;
        }
        cmd_send_ctl_status(&param->msg, &param->to_addr, 0x01);
        CORS_COMMUNICATE();
        logger_write_message(MESSAGE_LOGGER_FILE, "STATUS_RTK_ERROR");
    }
    cors_client_destroy(client);
    //release param
    destroy_param(&param);
    logger_write_message(MESSAGE_LOGGER_FILE, "task login cors end");
    logger_warn("task login cors end\n");
    return NULL;

}