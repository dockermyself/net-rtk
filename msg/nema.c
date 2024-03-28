//
// Created by server on 2022/4/19.
//

#include "nema.h"
#include "utility/log.h"


int NEMACheck(const char *nema, int len) {
    assert(nema != NULL);
    int sum = 0;
    for (int i = 1; i < len; ++i) {
        sum ^= nema[i];
    }
    return sum;
}

int NEMACheckMatch(const char *buffer,
                   int *buffer_start,
                   int package_end,
                   char *head_code,
                   int code_len) {
    assert(buffer != NULL &&
           buffer_start != NULL &&
           head_code != NULL &&
           code_len > 0);
    int check_code = 0;
    while (*buffer_start < package_end) {
        if (sscanf(buffer + package_end + 1, "%x", &check_code) > 0 &&
            NEMACheck(buffer + *buffer_start,
                      package_end - *buffer_start) == check_code) {
            return 1;
        }
        *buffer_start = NEMAFindHead(buffer, *buffer_start + 1,
                                      package_end, head_code, code_len);
    }
    printf("bbc check not match\n");
    return 0;
}

int NEMASplit(const char *nema, char *data, int str_len, int data_size) {
    assert(nema != NULL && data != NULL && str_len > 0 && data_size > 0);
    int begin = 0;
    int i;
    for (i = 0; nema[begin] != '*' && i < data_size; ++i) {
        begin++;
        sscanf(nema + begin, "%[^,*]", data + str_len * i);
        //        printf("%s\n", data + str_len * i);
        int len = (int) strlen(data + str_len * i);
        begin += len;

    }
    return i;
}

int NEMAParseRMC(const char *nema,
                 struct Latitude *lat,
                 struct Longitude *lon,
                 double *speed,
                 struct RTKTime *current_time) {
    assert(nema != NULL && lat != NULL && lon != NULL && speed != NULL && time != NULL);
    const int kArraySize = kRMCCheckSum;
    const int kStrLen = 20;
    const int kBufferLen = (kArraySize + 1) * kStrLen;
    char data[kBufferLen];
    memset(data, 0, kBufferLen);
    if (kArraySize != NEMASplit(nema, data, kStrLen, kArraySize + 1)) {
        LoggerWarn("split rmc failed\n");
        return 0;
    }
    const char *str_lat = data + kStrLen * kRMCLatitude;
    const char *str_lon = data + kStrLen * kRMCNorS;
    if (strlen(str_lat) == 0 || strlen(str_lon) == 0) {
//        LoggerWarn("lat or lon is null\n");
        return 0;
    }
    double lan_degree = strtod(data + kStrLen * kRMCLatitude, NULL);
    double lon_degree = strtod(data + kStrLen * kRMCLongitude, NULL);

    lat->degree = (int) (lan_degree / 100);
    lat->minute = lan_degree - lat->degree * 100;
    lat->ns = data[kStrLen * kRMCNorS];

    lon->degree = (int) (lon_degree / 100);
    lon->minute = lon_degree - lon->degree * 100;
    lon->ew = data[kStrLen * kRMCEorW];

    *speed = strtod(data + kStrLen * kRMCSpeed, NULL);

    const char *str_time = data + kStrLen * kRMCTime;
    if (strlen(str_time) == 0) {
        LoggerWarn("time is null\n");
        return 0;
    }
    //hhmmss.sss
    current_time->hour = (str_time[0] - '0') * 10 + (str_time[1] - '0');
    current_time->minute = (str_time[2] - '0') * 10 + (str_time[3] - '0');
    current_time->second = (float) strtod(str_time + 4, NULL);
    return 1;

}

// $GNHDT,143.7366,T*19
int NEMAParseHDT(const char *nema, double *heading) {
    assert(nema != NULL);
    const int kArraySize = kHDTCheckSum;
    const int kStrLen = 20;
    const int kBufferLen = (kArraySize + 1) * kStrLen;
    char data[kBufferLen];
    memset(data, 0, kBufferLen);

    if (kArraySize != NEMASplit(nema, data, kStrLen, kArraySize + 1)) {
        LoggerWarn("split hdt failed\n");
        return 0;
    }
    const char *str_heading = data + kStrLen * kHDTHeading;
    if (strlen(str_heading) == 0) {
//        LoggerWarn("heading is null\n");
        return 0;
    }
    *heading = strtod(str_heading, NULL);
    return 1;
}

// $GNGGA,092320.000,2519.0490,N,11024.8391,E,1,23,0.7,175.7,M,0.0,M,*7D
int NEMAParseGGA(const char *nema, struct LLA *outputs) {
    assert(nema != NULL);
    const int kArraySize = kGGACheckSum;
    const int kStrLen = 20;
    const int kBufferLen = (kArraySize + 1) * kStrLen;
    char data[kBufferLen];
    memset(data, 0, kBufferLen);
    if (kArraySize != NEMASplit(nema, data, kStrLen, kArraySize + 1)) {
        LoggerWarn("split gga failed\n");
        return 0;
    }
    const char *lat_str = data + kStrLen * kGGALatitude;
    const char *lon_str = data + kStrLen * kGGALongitude;
    if (strlen(lat_str) == 0 || strlen(lon_str) == 0) {
//        LoggerWarn("lat or lon is null\n");
        return 0;
    }
    if (outputs != NULL) {
        double lon = strtod(data + kStrLen * kGGALongitude, NULL);
        double lat = strtod(data + kStrLen * kGGALatitude, NULL);

        outputs->lat = floor(lat / 100) + (lat - (int) (lat / 100) * 100) / 60.0;
        if (data[kStrLen * kRMCNorS] == 'S') {
            outputs->lat = -outputs->lat;
        }
        outputs->lon = floor(lon / 100) + (lon - (int) (lon / 100) * 100) / 60.0;
        if (data[kStrLen * kRMCEorW] == 'W') {
            outputs->lon = -outputs->lon;
        }
        outputs->alt = strtod(data + kStrLen * kGGAAltitude, NULL);
    }

    return 1;
}

int NEMAFindHead(const char *buffer, int start, int end, char *head_code, int code_len) {
    assert(buffer != NULL);
    memset(head_code, 0, code_len);
    int dollar_index = -1;
    for (int i = start; i < end; ++i) {
        if (buffer[i] == '$') {
            dollar_index = i;
            for (int j = i + 1; j < end; ++j) {
                if (j - i > code_len || buffer[j] == '$') {//防止越界
                    LoggerWarn("nema head code is too long, serial message maybe mistake\n");
                    dollar_index = -1;//重新开始,不存在head大于code_len的情形
                    break;
                } else if (buffer[j] == ',') {
                    memcpy(head_code, buffer + i, j - i);
                    return i;
                }
            }
        }
    }
    if (dollar_index >= 0) {
        return dollar_index;
    }
    return end;
}

int NEMAFindTail(const char *buffer, int start, int end) {
    assert(buffer != NULL);
    const int extra_len = 4;
    for (int i = start; i < end - extra_len; ++i) {
        if (buffer[i] == '*') {
            return i;
        }
    }
    return end;
}

void NEMAFormatData(char *buffer, int start, int end, char *data, int *len) {
    assert(buffer != NULL && data != NULL && len != NULL);
    *len = end - start;
    memcpy(data, buffer + start, *len);
    data[*len] = '\r';
    data[*len + 1] = '\n';
    *len = *len + 2;
}

double GetLatitude(const struct Latitude *lat) {
    assert(lat != NULL);
    double degree = lat->degree + lat->minute / 60.0;
    if (lat->ns == 'S') {
        degree = -degree;
    }
    return degree;
}


double GetLongitude(const struct Longitude *lon) {
    assert(lon != NULL);
    double degree = lon->degree + lon->minute / 60.0;
    if (lon->ew == 'W') {
        degree = -degree;
    }
    return degree;
}

//t1 > t2
float GetTimeDiff(const struct RTKTime *t1, const struct RTKTime *t2) {
    assert(t1 != NULL && t2 != NULL);
    int hour = t1->hour >= t2->hour ? t1->hour - t2->hour : t1->hour + 24 - t2->hour;

    return (float) hour * 3600 + (float) (t1->minute - t2->minute) * 60 +
           (t1->second - t2->second);

}

//LLA TO ECEF
void LLAToECEF(double lat, double lon, double alt, double *x, double *y, double *z) {
    assert(x != NULL && y != NULL && z != NULL);
    const double a = 6378137.0;
    const double b = 6356752.314245;
    double e2 = 1 - (b * b) / (a * a);
    double N = a / sqrt(1 - e2 * pow(sin(lat), 2));
    *x = (N + alt) * cos(lat) * cos(lon);
    *y = (N + alt) * cos(lat) * sin(lon);
    *z = (N * (1 - e2) + alt) * sin(lat);
}


//LLA TO ENU
void LLAToENU(struct LLA *lla1, struct LLA *lla2, double *de, double *dn, double *du) {
    assert(lla1 != NULL && lla2 != NULL && de != NULL && dn != NULL && du != NULL);
    const double a = 6378137.0;
    const double b = 6356752.314245;
    const double R = (a + b) / 2;
    *de = (lla2->lon - lla1->lon) * M_PI / 180 * R * cos(lla1->lat * M_PI / 180);
    *dn = (lla2->lat - lla1->lat) * M_PI / 180 * R;
    *du = (lla2->alt - lla1->alt);
}

//LLA TO UTM
void LLAToUTM(struct LLA *lla, double *x, double *y) {
    assert(lla != NULL && x != NULL && y != NULL);
    const double a = 6378137.0;
    const double b = 6356752.314245;
    const double R = (a + b) / 2;
    *x = lla->lon * M_PI / 180 * R;
    *y = lla->lat * M_PI / 180 * R;
}