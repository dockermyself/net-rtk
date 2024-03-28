//
// Created by server on 2022/4/19.
//

#ifndef RTK_NAV_NEMA_H
#define RTK_NAV_NEMA_H

#include "include/header.h"


enum {
    kRMCTime = 1,
    kRMCStatus = 2,
    kRMCLatitude = 3,
    kRMCNorS = 4,
    kRMCLongitude = 5,
    kRMCEorW = 6,
    kRMCSpeed = 7,
    kRMCCourse = 8,
    kRMCData = 9,
    kRMCMagneticVariation = 10,
    kRMCMagneticVariationEW = 11,
    kRMCModeIndition = 12,
    kRMCCheckSum = 13,
};
enum {
    kGGATime = 1,
    kGGALatitude = 2,
    kGGANorS = 3,
    kGGALongitude = 4,
    kGGAEorW = 5,
    kGGAQuality = 6,
    kGGASatelliteNum = 7,
    kGGAHdop = 8,
    kGGAAltitude = 9,
    kGGAHeight = 11,
    kGGADiffTime = 13,
    kGGADiffStationID = 14,
    kGGACheckSum = 15,
};

enum {
    kHDTHeading = 1,
    kHDTTurn = 2,
    kHDTCheckSum = 3,
};

#pragma pack(1)
struct Latitude {
    int degree;
    double minute;
    uint8_t ns;
};
struct Longitude {
    int degree;
    double minute;
    uint8_t ew;
};
struct RTKTime {
    int hour;
    int minute;
    float second;
};

//WGS-84
struct LLA {
    double lon;
    double lat;
    double alt;
    float head;
    float speed;
};
#pragma pack()

double GetLatitude(const struct Latitude *lat);

double GetLongitude(const struct Longitude *lon);

float GetTimeDiff(const struct RTKTime *t1, const struct RTKTime *t2);

void LLAToECEF(double lat, double lon, double alt, double *x, double *y, double *z);

void LLAToENU(struct LLA *lla1, struct LLA *lla2, double *de, double *dn, double *du);

void LLAToUTM(struct LLA *lla, double *x, double *y);


int NEMACheck(const char *nema, int len);

int NEMACheckMatch(const char *buffer,
                   int *buffer_start,
                   int package_end,
                   char *head_code,
                   int code_len);

int NEMAParseRMC(const char *nema,
                 struct Latitude *lat,
                 struct Longitude *lon,
                 double *speed,
                 struct RTKTime *time);

int NEMAParseHDT(const char *nema, double *heading);

int NEMAParseGGA(const char *nema, struct LLA *outputs);

int NEMAFindHead(const char *buffer, int start, int end, char *head, int head_len);

int NEMAFindTail(const char *buffer, int start, int end);

void NEMAFormatData(char *buffer, int start, int end, char *nema_data, int *len);

#endif // RTK_NAV_NEMA_H
