void convertDate(double date, char* buffer, int buffersize);
bool wifiConnect();
bool setClock();
bool getValue(int &sgv, time_t &date);
byte dstOffset(byte d, byte m, unsigned int y, byte h);
