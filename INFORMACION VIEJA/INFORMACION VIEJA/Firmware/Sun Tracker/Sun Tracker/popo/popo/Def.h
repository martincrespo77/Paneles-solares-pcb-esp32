//================================================================================================
// DEFINICIÓN DE VARIABLES PARA EL KALMAN
// --------------------------------------

unsigned int ii;

unsigned char adc_dato_lo;
unsigned char adc_dato_hi;

int adc_dato_0;
int adc_dato_1;
int adc_dato_2;
int adc_dato_3;

int error_azim;
int error_elev;

int Int_elev;
int Int_azim;


//================================================================================================


//================================================================================================
// DEFINICIÓN DE VARIABLES PARA EL PID
// -----------------------------------
unsigned char sent;

int PWM_El;
int PWM_Az;

double Error_El;
double Error_Az;

double PID_El;
double PID_Az;

double Integral_El;
double Integral_Az;
double Reset_aw;

double Kp_El;
double Kd_El;
double Ki_El;
double Kp_Az;
double Kd_Az;
double Ki_Az;






