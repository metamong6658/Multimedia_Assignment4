#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <vector>
#include <cmath>
#include <cstring>
#define DATA_OFFSET_OFFSET 0x000A
#define WIDTH_OFFSET 0x0012
#define HEIGHT_OFFSET 0x0016
#define BITS_PER_PIXEL_OFFSET 0x001C
#define HEADER_SIZE 14
#define INFO_HEADER_SIZE 40
#define NO_COMPRESION 0
#define MAX_NUMBER_OF_COLORS 0
#define ALL_COLORS_REQUIRED 0

typedef unsigned int int32;
typedef short int16;
typedef unsigned char byte;

struct outpixel {
    byte* outpixels;
};

typedef struct _bitfiled {
    unsigned b0 : 1;
    unsigned b1 : 1;
    unsigned b2 : 1;
    unsigned b3 : 1;
    unsigned b4 : 1;
    unsigned b5 : 1;
    unsigned b6 : 1;
    unsigned b7 : 1;
} bitfiled;

struct RLC {
    int runlength = 0;
    int value = 0;
};

struct MB{
    int16 pixel[8][8] = {0};
    int16 vec[64] = {0};
    std::vector<RLC> RLCArray;
};

struct Img {
    std::vector<MB> MBArray;
    std::vector<int> DPCMArray;
    std::vector<short> moVecX;
    std::vector<short> moVecY;
};

std::vector<Img> ImageArray;
std::vector<Img> ImageArray2;
std::vector<MB> MBArray;
std::vector<int> DPCMArray;
std::vector<outpixel> outpixelArray;
std::string codeDC = "";
std::string codeAC = "";
std::string code = "";
const double PI = std::acos(-1);
int16 rframe[288][352] = {0}; // memory
int16 tframe[288][352] = {0}; // current
int16 sframe[288][352] = {0}; // estimated 
int16 dframe[288][352] = {0}; // difference

void ReadImage(const char* fileName, const char* outfileName, byte** pixels, int32* width, int32* height, int32* bytesPerPixel, FILE*& imageFile, FILE*& OUT)
{
    imageFile = fopen(fileName, "rb");
    int32 dataOffset;
    int32 LookUpTable=0; 
    fseek(imageFile, HEADER_SIZE + INFO_HEADER_SIZE-8, SEEK_SET);
    fread(&LookUpTable, 4, 1, imageFile);
    fseek(imageFile, 0, SEEK_SET);

    OUT = fopen(outfileName, "wb");

    int header = 0;
    if (LookUpTable)
        header = HEADER_SIZE + INFO_HEADER_SIZE + 1024;
    else
        header = HEADER_SIZE + INFO_HEADER_SIZE;
    for (int i = 0; i < header; i++)
    {
        int get = getc(imageFile);
        putc(get, OUT);
    }

    fseek(imageFile, DATA_OFFSET_OFFSET, SEEK_SET);
    fread(&dataOffset, 4, 1, imageFile);
    fseek(imageFile, WIDTH_OFFSET, SEEK_SET);
    fread(width, 4, 1, imageFile);
    fseek(imageFile, HEIGHT_OFFSET, SEEK_SET);
    fread(height, 4, 1, imageFile);
    int16 bitsPerPixel;
    fseek(imageFile, BITS_PER_PIXEL_OFFSET, SEEK_SET);
    fread(&bitsPerPixel, 2, 1, imageFile);
    *bytesPerPixel = ((int32)bitsPerPixel) / 8; //3 bytes per pixel when color, 1 byte per pixel when grayscale

    int paddedRowSize = (int)(4 * (float)(*width) / 4.0f) * (*bytesPerPixel);
    int unpaddedRowSize = (*width) * (*bytesPerPixel);
    int totalSize = unpaddedRowSize * (*height);

    *pixels = new byte[totalSize];
    int i = 0;
    byte* currentRowPointer = *pixels + ((*height - 1) * unpaddedRowSize);
    for (i = 0; i < *height; i++)
    {
        fseek(imageFile, dataOffset + (i * paddedRowSize), SEEK_SET);
        fread(currentRowPointer, 1, unpaddedRowSize, imageFile);
        currentRowPointer -= unpaddedRowSize;
    }
    fclose(imageFile);
}

void WriteImage(byte* pixels, int32 width, int32 height, int32 bytesPerPixel, FILE*& outputFile)
{
    int unpaddedRowSize = width * bytesPerPixel;
    for (int i = 0; i < height; i++)
    {
        int pixelOffset = ((height - i) - 1) * unpaddedRowSize;
        for(int j=0;j<unpaddedRowSize/3;j++) {
            fwrite(&pixels[pixelOffset+3*j], 1, 1, outputFile);
            fwrite(&pixels[pixelOffset+3*j], 1, 1, outputFile);
            fwrite(&pixels[pixelOffset+3*j], 1, 1, outputFile);
        }
    }
    fclose(outputFile);
} // Just check for BMP file is 24bits or 8bits

void readMB(byte* pixels, int32 width, int32 height, int32 bytesPerPixel)
{
    int unpaddedRowSize = width * bytesPerPixel;
    MB tempMB;
    int start;
    int col;
    int row;
    for(row=0;row<height;row=row+8)
    {
        for(col=0;col<unpaddedRowSize;col=col+24)
        {
            start = unpaddedRowSize * row + col;
            for(int i=0;i<8;i++)
            {
                for(int j=0;j<8;j++)
                {
                    tempMB.pixel[i][j] = (int16)pixels[start+unpaddedRowSize*i+j*3];
                }
            }
            MBArray.push_back(tempMB);
        }
    }
} // read raw data to Macro Block

void writeMB(std::vector<MB> MBarray, byte* pixels, int32 width, int32 height, int32 bytesPerPixel) {
    for(int i=0;i<height;i=i+8) // Pixels Change
        {
            int start;
            int unpaddedRowSize = width * bytesPerPixel;
            for(int j=0;j<unpaddedRowSize;j=j+24)
            {
                start = unpaddedRowSize * i + j;
                MB temp = MBarray[0];
                for(int x=0;x<8;x++)
                {
                    for(int y=0;y<8;y++)
                    {
                        pixels[start+unpaddedRowSize*x+y*3] = temp.pixel[x][y];
                    }
                }
                MBarray.erase(MBarray.begin());
            }
        }
}

void cleanMB() {
    MBArray.clear();
}

void zigzag(std::vector<MB> mbarray) {
    for(int l=0;l<mbarray.size();l++)
    {
        MB tempMB;
        int sw = 0;
        int row = 0;
        int col = 0;
        tempMB = mbarray[l];
        for(int i=0; i<64; i++) // 64 times iteration
        {
            tempMB.vec[i] = tempMB.pixel[row][col];
            if(sw == 0) {
                if(col == 7) {
                    row++;
                    sw = 1;
                }
                else {
                    if(row == 0)
                    {
                        col++;
                        sw = 1;
                    }
                    else {
                        row--;
                        col++;
                    }
                }
            }
            else {
                if(row == 7) {
                    col++;
                    sw = 0;
                }
                else {
                    if(col == 0)
                    {
                        row++;
                        sw = 0;
                    }
                    else {
                        col--;
                        row++;
                    }
                }
            }
        }
        mbarray[l] = tempMB;
    }
    MBArray = mbarray;
}

void DPCM(std::vector<MB> mbarray) {
    DPCMArray.push_back(mbarray[0].vec[0]);
    for(int l=1; l<mbarray.size(); l++)
    {
        DPCMArray.push_back(mbarray[l].vec[0] - mbarray[l-1].vec[0]);
    }
}

void rlcoding(std::vector<MB> mbarray) {
    int rlcvec[63];
    int rlcCNT = 0;
    RLC temp;
    for(int i=0;i<mbarray.size();i++) {
        for(int j=1;j<64;j++) {
            rlcvec[j-1] = mbarray[i].vec[j];
        }
        rlcCNT = 0;
        for(int j=0;j<63;j++) {
            if(rlcCNT == 15) {
                temp.runlength = rlcCNT;
                temp.value = rlcvec[j];
                rlcCNT = 0;
                mbarray[i].RLCArray.push_back(temp);
            }
            else if(rlcvec[j] != 0) {
                temp.runlength = rlcCNT;
                temp.value = rlcvec[j];
                rlcCNT = 0;
                mbarray[i].RLCArray.push_back(temp);
            }
            else {
                rlcCNT++;
            }
        }
    }

    for(int i=0; i<mbarray.size(); i++) {
        for(int j = mbarray[i].RLCArray.size()-1; j>0; j--) {
            if(mbarray[i].RLCArray[j-1].value == 0 && mbarray[i].RLCArray[j].value == 0) {
                mbarray[i].RLCArray.erase(mbarray[i].RLCArray.begin() + j);
            }
            else if(mbarray[i].RLCArray[j].value == 0) {
                mbarray[i].RLCArray[j].runlength = 0;
                break;
            }
            if(mbarray[i].RLCArray.size() == 1) {
                mbarray[i].RLCArray[0].runlength = 0;
            }
        }
    }

    MBArray = mbarray;
}

int getsize(int value) {
    int ans;
    int temp = abs(value);
    if(temp == 0) {
        ans = 0;
    }
    else if(temp <= 1) {
        ans = 1;
    }
    else if(temp <= 3) {
        ans = 2;
    }
    else if(temp <= 7) {
        ans = 3;
    }
    else if(temp <= 15) {
        ans = 4;
    }
    else if(temp <= 31) {
        ans = 5;
    }
    else if(temp <= 63) {
        ans = 6;
    }
    else if(temp <= 127) {
        ans = 7;
    }
    else if(temp <= 255) {
        ans = 8;
    }
    else if(temp <= 511) {
        ans = 9;
    }
    else if(temp <= 1023) {
        ans = 10;
    }
    else if(temp <= 2047) {
        ans = 11;
    }
    else if(temp <= 4095) {
        ans = 12;
    }
    else if(temp <= 8191) {
        ans = 13;
    }
    else if(temp <= 16383) {
        ans = 14;
    }
    else if(temp <= 32767) {
        ans = 15;
    }
    return ans;
}

std::string DCHUF(int size) {
    std::string str;
    if(size == 0) {
        str = "00";
    }
    else if(size == 1) {
        str = "010";
    }
    else if(size == 2) {
        str = "011";
    }
    else if(size == 3) {
        str = "100";
    }
    else if(size == 4) {
        str = "101";
    }
    else if(size == 5) {
        str = "110";
    }
    else if(size == 6) {
        str = "1110";
    }
    else if(size == 7) {
        str = "11110";
    }
    else if(size == 8) {
        str = "111110";
    }
    else if(size == 9) {
        str = "1111110";
    }
    else if(size == 10) {
        str = "11111110";
    }
    else if(size == 11) {
        str = "111111110";
    }
    return str;
}

std::string getpost(int size, int value) {
    std::string str = "";
    if(value == 0) {
        str.append("0");
    }
    else if(value > 0) {
        while(1) {
            if(value == 1) {
                str.append("1");
                break;
            }
            else {
                str.append(std::to_string(value%2));
                value = value / 2;
            }
        }
        char *a = new char[size];
        strcpy(a,str.c_str());
        for(int j=0;j<size;j++) {
            str[j] = a[size-1-j]; 
        }
        delete[] a;
    }
    else {
        value = abs(value);
        while(1) {
            if(value == 1) {
                str.append("0");
                break;
            }
            else {
                int temp = 0;
                if(value%2 == 0) {
                    temp = 1;
                }
                else {
                    temp = 0;
                }
                str.append(std::to_string(temp));
                value = value / 2;
            }
        }
        char *a = new char[size];
        strcpy(a,str.c_str());
        for(int j=0;j<size;j++) {
            str[j] = a[size-1-j]; 
        }
        delete[] a;
    }

    return str;
}

void getDCcode() {
    int size;
    std::string str;
    std::string str2;
    for(int i=0; i<DPCMArray.size();i++) {
        size = getsize(DPCMArray[i]);
        str = DCHUF(size);
        std::cout << "check\n";
        str2 = getpost(size, DPCMArray[i]);
        std::cout << "check2\n";
        codeDC.append(str);
        codeDC.append(str2);
    }
    
    DPCMArray.clear();
}

std::string ACHUF(int runlength, int size) {
    std::string str;
    if(runlength == 0 && size == 0) {
        str = "1010";
    }
    else if(runlength == 0 && size == 1) {
        str = "00";
    }
    else if(runlength == 0 && size == 2) {
        str = "01";
    }
    else if(runlength == 0 && size == 3) {
        str = "100";
    }
    else if(runlength == 0 && size == 4) {
        str = "1011";
    }
    else if(runlength == 0 && size == 5) {
        str = "11010";
    }
    else if(runlength == 0 && size == 6) {
        str = "1111000";
    }
    else if(runlength == 0 && size == 7) {
        str = "11111000";
    }
    else if(runlength == 0 && size == 8) {
        str = "1111110110";
    }
    else if(runlength == 0 && size == 9) {
        str = "1111111110000010";
    }
    else if(runlength == 0 && size == 10) {
        str = "1111111110000011";
    }
    else if(runlength == 1 && size == 1) {
        str = "1100";
    }
    else if(runlength == 1 && size == 2) {
        str = "11011";
    }
    else if(runlength == 1 && size == 3) {
        str = "1111001";
    }
    else if(runlength == 1 && size == 4) {
        str = "111110110";
    }
    else if(runlength == 1 && size == 5) {
        str = "11111110110";
    }
    else if(runlength == 1 && size == 6) {
        str = "1111111110000100";
    }
    else if(runlength == 1 && size == 7) {
        str = "1111111110000101";
    }
    else if(runlength == 1 && size == 8) {
        str = "1111111110000110";
    }
    else if(runlength == 1 && size == 9) {
        str = "1111111110000111";
    }
    else if(runlength == 1 && size == 10) {
        str = "1111111110001000";
    }
    else if(runlength == 2 && size == 1) {
        str = "11100";
    }
    else if(runlength == 2 && size == 2) {
        str = "11111001";
    }
    else if(runlength == 2 && size == 3) {
        str = "1111110111";
    }
    else if(runlength == 2 && size == 4) {
        str = "111111110100";
    }
    else if(runlength == 2 && size == 5) {
        str = "1111111110001001";
    }
    else if(runlength == 2 && size == 6) {
        str = "1111111110001010";
    }
    else if(runlength == 2 && size == 7) {
        str = "1111111110001011";
    }
    else if(runlength == 2 && size == 8) {
        str = "1111111110001100";
    }
    else if(runlength == 2 && size == 9) {
        str = "1111111110001101";
    }
    else if(runlength == 2 && size == 10) {
        str = "1111111110001110";
    }
    else if(runlength == 3 && size == 1) {
        str = "111010";
    }
    else if(runlength == 3 && size == 2) {
        str = "111110111";
    }
    else if(runlength == 3 && size == 3) {
        str = "111111110101";
    }
    else if(runlength == 3 && size == 4) {
        str = "1111111110001111";
    }
    else if(runlength == 3 && size == 5) {
        str = "1111111110010000";
    }
    else if(runlength == 3 && size == 6) {
        str = "1111111110010001";
    }
    else if(runlength == 3 && size == 7) {
        str = "1111111110010010";
    }
    else if(runlength == 3 && size == 8) {
        str = "1111111110010011";
    }
    else if(runlength == 3 && size == 9) {
        str = "1111111110010100";
    }
    else if(runlength == 3 && size == 10) {
        str = "1111111110010101";
    }
    else if(runlength == 4 && size == 1) {
        str = "111011";
    }
    else if(runlength == 4 && size == 2) {
        str = "1111111000";
    }
    else if(runlength == 4 && size == 3) {
        str = "1111111110010110";
    }
    else if(runlength == 4 && size == 4) {
        str = "1111111110010111";
    }
    else if(runlength == 4 && size == 5) {
        str = "1111111110011000";
    }
    else if(runlength == 4 && size == 6) {
        str = "1111111110011001";
    }
    else if(runlength == 4 && size == 7) {
        str = "1111111110011010";
    }
    else if(runlength == 4 && size == 8) {
        str = "1111111110011011";
    }
    else if(runlength == 4 && size == 9) {
        str = "1111111110011100";
    }
    else if(runlength == 4 && size == 10) {
        str = "1111111110011101";
    }
    else if(runlength == 5 && size == 1) {
        str = "1111010";
    }
    else if(runlength == 5 && size == 2) {
        str = "11111110111";
    }
    else if(runlength == 5 && size == 3) {
        str = "1111111110011110";
    }
    else if(runlength == 5 && size == 4) {
        str = "1111111110011111";
    }
    else if(runlength == 5 && size == 5) {
        str = "1111111110100000";
    }
    else if(runlength == 5 && size == 6) {
        str = "1111111110100001";
    }
    else if(runlength == 5 && size == 7) {
        str = "1111111110100010";
    }
    else if(runlength == 5 && size == 8) {
        str = "1111111110100011";
    }
    else if(runlength == 5 && size == 9) {
        str = "1111111110100100";
    }
    else if(runlength == 5 && size == 10) {
        str = "1111111110100101";
    }
    else if(runlength == 6 && size == 1) {
        str = "1111011";
    }
    else if(runlength == 6 && size == 2) {
        str = "111111110110";
    }
    else if(runlength == 6 && size == 3) {
        str = "1111111110100110";
    }
    else if(runlength == 6 && size == 4) {
        str = "1111111110100111";
    }
    else if(runlength == 6 && size == 5) {
        str = "1111111110101000";
    }
    else if(runlength == 6 && size == 6) {
        str = "1111111110101001";
    }
    else if(runlength == 6 && size == 7) {
        str = "1111111110101010";
    }
    else if(runlength == 6 && size == 8) {
        str = "1111111110101011";
    }
    else if(runlength == 6 && size == 9) {
        str = "1111111110101100";
    }
    else if(runlength == 6 && size == 10) {
        str = "1111111110101101";
    }
    else if(runlength == 7 && size == 1) {
        str = "11111010";
    }
    else if(runlength == 7 && size == 2) {
        str = "111111110111";
    }
    else if(runlength == 7 && size == 3) {
        str = "1111111110101110";
    }
    else if(runlength == 7 && size == 4) {
        str = "1111111110101111";
    }
    else if(runlength == 7 && size == 5) {
        str = "1111111110110000";
    }
    else if(runlength == 7 && size == 6) {
        str = "1111111110110001";
    }
    else if(runlength == 7 && size == 7) {
        str = "1111111110110010";
    }
    else if(runlength == 7 && size == 8) {
        str = "1111111110110011";
    }
    else if(runlength == 7 && size == 9) {
        str = "1111111110110100";
    }
    else if(runlength == 7 && size == 10) {
        str = "1111111110110101";
    }
    else if(runlength == 8 && size == 1) {
        str = "111111000";
    }
    else if(runlength == 8 && size == 2) {
        str = "111111111000000";
    }
    else if(runlength == 8 && size == 3) {
        str = "1111111110110110";
    }
    else if(runlength == 8 && size == 4) {
        str = "1111111110110111";
    }
    else if(runlength == 8 && size == 5) {
        str = "1111111110111000";
    }
    else if(runlength == 8 && size == 6) {
        str = "1111111110111001";
    }
    else if(runlength == 8 && size == 7) {
        str = "1111111110111010";
    }
    else if(runlength == 8 && size == 8) {
        str = "1111111110111011";
    }
    else if(runlength == 8 && size == 9) {
        str = "1111111110111100";
    }
    else if(runlength == 8 && size == 10) {
        str = "1111111110111101";
    }
    else if(runlength == 9 && size == 1) {
        str = "111111001";
    }
    else if(runlength == 9 && size == 2) {
        str = "1111111110111110";
    }
    else if(runlength == 9 && size == 3) {
        str = "1111111110111111";
    }
    else if(runlength == 9 && size == 4) {
        str = "1111111111000000";
    }
    else if(runlength == 9 && size == 5) {
        str = "1111111111000001";
    }
    else if(runlength == 9 && size == 6) {
        str = "1111111111000010";
    }
    else if(runlength == 9 && size == 7) {
        str = "1111111111000011";
    }
    else if(runlength == 9 && size == 8) {
        str = "1111111111000100";
    }
    else if(runlength == 9 && size == 9) {
        str = "1111111111000101";
    }
    else if(runlength == 9 && size == 10) {
        str = "1111111111000110";
    }
    else if(runlength == 10 && size == 1) {
        str = "111111010";
    }
    else if(runlength == 10 && size == 2) {
        str = "1111111111000111";
    }
    else if(runlength == 10 && size == 3) {
        str = "1111111111001000";
    }
    else if(runlength == 10 && size == 4) {
        str = "1111111111001001";
    }
    else if(runlength == 10 && size == 5) {
        str = "1111111111001010";
    }
    else if(runlength == 10 && size == 6) {
        str = "1111111111001011";
    }
    else if(runlength == 10 && size == 7) {
        str = "1111111111001100";
    }
    else if(runlength == 10 && size == 8) {
        str = "1111111111001101";
    }
    else if(runlength == 10 && size == 9) {
        str = "1111111111001110";
    }
    else if(runlength == 10 && size == 10) {
        str = "1111111111001111";
    }
    else if(runlength == 11 && size == 1) {
        str = "1111111001";
    }
    else if(runlength == 11 && size == 2) {
        str = "1111111111010000";
    }
    else if(runlength == 11 && size == 3) {
        str = "1111111111010001";
    }
    else if(runlength == 11 && size == 4) {
        str = "1111111111010010";
    }
    else if(runlength == 11 && size == 5) {
        str = "1111111111010011";
    }
    else if(runlength == 11 && size == 6) {
        str = "1111111111010100";
    }
    else if(runlength == 11 && size == 7) {
        str = "1111111111010101";
    }
    else if(runlength == 11 && size == 8) {
        str = "1111111111010110";
    }
    else if(runlength == 11 && size == 9) {
        str = "1111111111010111";
    }
    else if(runlength == 11 && size == 10) {
        str = "1111111111011000";
    }
    else if(runlength == 12 && size == 1) {
        str = "1111111010";
    }
    else if(runlength == 12 && size == 2) {
        str = "1111111111011001";
    }
    else if(runlength == 12 && size == 3) {
        str = "1111111111011010";
    }
    else if(runlength == 12 && size == 4) {
        str = "1111111111011011";
    }
    else if(runlength == 12 && size == 5) {
        str = "1111111111011100";
    }
    else if(runlength == 12 && size == 6) {
        str = "1111111111011101";
    }
    else if(runlength == 12 && size == 7) {
        str = "1111111111011110";
    }
    else if(runlength == 12 && size == 8) {
        str = "1111111111011111";
    }
    else if(runlength == 12 && size == 9) {
        str = "1111111111100000";
    }
    else if(runlength == 12 && size == 10) {
        str = "1111111111100001";
    }
    else if(runlength == 13 && size == 1) {
        str = "11111111000";
    }
    else if(runlength == 13 && size == 2) {
        str = "1111111111100010";
    }
    else if(runlength == 13 && size == 3) {
        str = "1111111111100011";
    }
    else if(runlength == 13 && size == 4) {
        str = "1111111111100100";
    }
    else if(runlength == 13 && size == 5) {
        str = "1111111111100101";
    }
    else if(runlength == 13 && size == 6) {
        str = "1111111111100110";
    }
    else if(runlength == 13 && size == 7) {
        str = "1111111111100111";
    }
    else if(runlength == 13 && size == 8) {
        str = "1111111111101000";
    }
    else if(runlength == 13 && size == 9) {
        str = "1111111111101001";
    }
    else if(runlength == 13 && size == 10) {
        str = "1111111111101010";
    }
    else if(runlength == 14 && size == 1) {
        str = "1111111111101011";
    }
    else if(runlength == 14 && size == 2) {
        str = "1111111111101100";
    }
    else if(runlength == 14 && size == 3) {
        str = "1111111111101101";
    }
    else if(runlength == 14 && size == 4) {
        str = "1111111111101110";
    }
    else if(runlength == 14 && size == 5) {
        str = "1111111111101111";
    }
    else if(runlength == 14 && size == 6) {
        str = "1111111111110000";
    }
    else if(runlength == 14 && size == 7) {
        str = "1111111111110001";
    }
    else if(runlength == 14 && size == 8) {
        str = "1111111111110010";
    }
    else if(runlength == 14 && size == 9) {
        str = "1111111111110011";
    }
    else if(runlength == 14 && size == 10) {
        str = "1111111111110100";
    }
    else if(runlength == 15 && size == 1) {
        str = "1111111111110101";
    }
    else if(runlength == 15 && size == 2) {
        str = "1111111111110110";
    }
    else if(runlength == 15 && size == 3) {
        str = "1111111111110111";
    }
    else if(runlength == 15 && size == 4) {
        str = "1111111111111000";
    }
    else if(runlength == 15 && size == 5) {
        str = "1111111111111001";
    }
    else if(runlength == 15 && size == 6) {
        str = "1111111111111010";
    }
    else if(runlength == 15 && size == 7) {
        str = "1111111111111011";
    }
    else if(runlength == 15 && size == 8) {
        str = "1111111111111100";
    }
    else if(runlength == 15 && size == 9) {
        str = "1111111111111101";
    }
    else if(runlength == 15 && size == 10) {
        str = "1111111111111110";
    }
    else if(runlength == 15 && size == 0) {
        str = "11111111001";
    }
    return str;
}

void getACcode(std::vector<MB> mbarray) {
    int size;
    std::string str;
    std::string str2;
    for(int i=0;i<mbarray.size();i++) {
        for(int j=0;j<mbarray[i].RLCArray.size(); j++) {
            size = getsize(mbarray[i].RLCArray[j].value);
            str = ACHUF(mbarray[i].RLCArray[j].runlength, size);
            str2 = getpost(size, mbarray[i].RLCArray[j].value);
            int rlc_length = mbarray[i].RLCArray.size();
            codeAC.append(std::to_string(rlc_length)); // 4-bytes
            codeAC.append(str);
            codeAC.append(str2);
        }
    }
}

void VLE(int frame_num) { 
    zigzag(ImageArray[frame_num].MBArray);
    ImageArray[frame_num].MBArray = MBArray;
    cleanMB();
    DPCM(ImageArray[frame_num].MBArray);
    ImageArray[frame_num].DPCMArray = DPCMArray;
    rlcoding(ImageArray[frame_num].MBArray);
    ImageArray[frame_num].MBArray = MBArray;
    cleanMB();

    getDCcode();
    
    unsigned char DCmargin = 8 - codeDC.size() % 8;
    if(DCmargin != 0) {
        for(int i=0; i<DCmargin; i++) {
            codeDC.append("0");
        }
    } // marginal bits
    int length_DC = codeDC.size();
    code.append(std::to_string(length_DC)); // 4-bytes
    code.append(codeDC);
    codeDC.clear();
    codeDC = ""; // refresh

    getACcode(ImageArray[frame_num].MBArray);
    unsigned char ACmargin = 8 - codeAC.size() % 8;
    if(ACmargin != 0) {
        for(int i=0; i< ACmargin; i++) {
            codeAC.append("0");
        }
    } // marginal bits
    int length_AC = codeAC.size();
    code.append(std::to_string(length_AC)); // 4-bytes
    code.append(codeAC);
    codeAC.clear();
    codeAC = ""; // refresh

    if(frame_num %5 != 0) { // inter-frame
        int length_mv = 2 * ImageArray[frame_num].moVecX.size(); 
        code.append(std::to_string(length_mv)); // 4-bytes
        for(int i=0; i<ImageArray[frame_num].moVecX.size(); i++) {
            code.append(std::to_string(ImageArray[frame_num].moVecX[i])); // short - 2bytes
            code.append(std::to_string(ImageArray[frame_num].moVecY[i])); // short - 2bytes
        }
    }
}

void Irlcoding(std::vector<MB> mbarray) {
    for(int i=0;i<mbarray.size();i++) { // Decompress RLC 
        int temp = 0;
        for(int j=0;j<mbarray[i].RLCArray.size();j++) {
            temp += (mbarray[i].RLCArray[j].runlength + 1);
            mbarray[i].vec[temp] = mbarray[i].RLCArray[j].value;
        }
        temp = 0;
    }
    MBArray = mbarray;
}

void IDPCM(std::vector<int> DPCMArray, std::vector<MB> mbarray) {
    for(int i=1;i<DPCMArray.size();i++) { // Decompress DPCM
            DPCMArray[i] = DPCMArray[i] + DPCMArray[i-1];
            mbarray[i].vec[0] = DPCMArray[i];
    }
    MBArray = mbarray;
}

void Izigzag(std::vector<MB> mbarray) {
    for(int l=0;l<mbarray.size();l++) { // Decompress Zig-Zag
            MB tempMB;
            int sw = 0;
            int row = 0;
            int col = 0;
            tempMB = mbarray[l];
    
            for(int i=0; i<64; i++) // 64 times iteration
            {
                tempMB.pixel[row][col] = tempMB.vec[i];
                if(sw == 0) {
                    if(col == 7) {
                        row++;
                        sw = 1;
                    }
                    else {
                        if(row == 0)
                        {
                            col++;
                            sw = 1;
                        }
                        else {
                            row--;
                            col++;
                        }
                    }
                }
                else {
                    if(row == 7) {
                        col++;
                        sw = 0;
                    }
                    else {
                        if(col == 0)
                        {
                            row++;
                            sw = 0;
                        }
                        else {
                            col--;
                            row++;
                        }
                    }
                }
            }
            mbarray[l] = tempMB;
        }
        MBArray = mbarray;
}

void IVLE(int frame_num) {
    Irlcoding(ImageArray2[frame_num].MBArray);
    ImageArray2[frame_num].MBArray = MBArray;
    cleanMB();
    IDPCM(ImageArray2[frame_num].DPCMArray,ImageArray2[frame_num].MBArray);
    ImageArray2[frame_num].MBArray = MBArray;
    cleanMB();
    Izigzag(ImageArray2[frame_num].MBArray);
    ImageArray2[frame_num].MBArray = MBArray;
    cleanMB();
}

void DCT(std::vector<MB> mbarray) {
    MB tempMB;
    MB tempMB2;
    float Fuv = 0.0;
    float sumf = 0.0;
    float Cu = 0.0;
    float Cv = 0.0;
    for(int l = 0; l<mbarray.size(); l++)
    {
        tempMB = mbarray[l];
        for(int v=0;v<8;v++)
        {
            if(v == 0) {
                Cv = sqrt(0.5);
            }
            else {
                Cv = 1;
            }
            for(int u=0;u<8;u++)
            {
                if(u == 0) {
                    Cu = sqrt(0.5);
                }
                else {
                    Cu = 1;
                }
                for(int y=0;y<8;y++)
                {
                    for(int x=0;x<8;x++)
                    {
                        sumf = sumf + (tempMB.pixel[x][y]-128) * cos((2.0*x+1.0)*PI*u/16.0) * cos((2.0*y+1.0)*PI*v/16.0);
                    }
                }
                Fuv = 0.25*Cu*Cv*sumf;
                tempMB2.pixel[u][v] = (int)Fuv; // Rounding
                sumf = 0.0; // Refresh
            }
        }
        mbarray[l] = tempMB2;
    }
    MBArray = mbarray;
} // 2Dimensional Discrete Cosine Transform

void Quantize(std::vector<MB> mbarray, int quantize_factor) {
    for(int i=0; i<mbarray.size(); i++) {
        for(int x=0;x<8;x++) {
            for(int y=0; y<8; y++) {
                mbarray[i].pixel[x][y] /= quantize_factor;
            }
        }
    }
    MBArray = mbarray;
}

void IDCT(std::vector<MB> mbarray) {
    for(int i=0;i<mbarray.size();i++) { // IDCD
            MB tempMB;
            MB tempMB2;
            float Fuv = 0.0;
            float sumf = 0.0;
            float Cu = 0.0;
            float Cv = 0.0;
            tempMB = mbarray[i];
            for(int y=0;y<8;y++)
            {
                for(int x=0;x<8;x++)
                {
                    for(int v=0;v<8;v++)
                    {
                        if(v == 0) {
                            Cv = sqrt(0.5);
                        }
                        else {
                            Cv = 1;
                        }
                        for(int u=0;u<8;u++)
                        {
                            if(u == 0) {
                                Cu = sqrt(0.5);
                            }
                            else {
                                Cu = 1;
                            }
                            sumf += + 0.25*Cu*Cv*((tempMB.pixel[u][v]) * cos((2.0*x+1.0)*PI*u/16.0) * cos((2.0*y+1.0)*PI*v/16.0));
                        }
                    }
                    tempMB2.pixel[x][y] = (int)sumf; // Rounding
                    sumf = 0.0; // Refresh
                }
            }
            mbarray[i] = tempMB2;
        }
        MBArray = mbarray;
}

void IQuantize(std::vector<MB> mbarray, int quantize_factor) {
    for(int i=0; i<mbarray.size(); i++) {
        for(int x=0; x<8; x++) {
            for(int y=0; y<8; y++) {
                mbarray[i].pixel[x][y] *= quantize_factor;
            }
        }
    }
    MBArray = mbarray;
}

void get_tframe(std::vector<MB> mbarray) {
    int row, col;
    int cnt = 0;
    for(row=0; row<288; row+=8) {
        for(col = 0; col<352; col+=8) {
            for(int y=0; y<8; y++) {
                for(int x=0; x<8; x++) {
                    tframe[row+y][col+x] = mbarray[cnt].pixel[y][x];
                }
            }
            cnt++;
        }
    }
}

void Div_ImgBlock_MV(int16 (*data)[352], unsigned char ** block, int m, int n)
{
	for (int i = 0; i < 16; i++)
	{
		for (int j = 0; j < 16; j++)
		{
			block[i][j] = data[m + i][n + j];
		}
	}
}

void MVpredict(int frame_num) {
    get_tframe(ImageArray[frame_num].MBArray);
    ImageArray[frame_num].moVecX.clear();
    ImageArray[frame_num].moVecY.clear();

	short mv_x = 0, mv_y = 0, x = 0, y = 0;
	unsigned char ** cp_block_r = new unsigned char *[16];
	for (int i = 0; i < 16; i++)
		cp_block_r[i] = new unsigned char[16];
	unsigned char ** cp_block_t = new unsigned char *[16];
	for (int i = 0; i < 16; i++)
		cp_block_t[i] = new unsigned char[16];


	for (int i = 0; i < 288 / 16; i++)
	{
		for (int j = 0; j < 352 / 16; j++)
		{
			x = j * 16;
			y = i * 16;
			int diff = INT_MAX;
			short temp_mv_x = 0;
			short temp_mv_y = 0;
			for (mv_y = -1 * 15; mv_y < 15; mv_y++)
			{
				for (mv_x = -1 * 15; mv_x < 15; mv_x++)
				{
					if (((y + mv_y) >= 0) && ((y + mv_y) <= (288 - 16)) && ((x + mv_x) >= 0) && ((x + mv_x) <= (352 - 16)))
					{
						int temp = 0;
						Div_ImgBlock_MV(rframe, cp_block_r, y + mv_y, x + mv_x);
						Div_ImgBlock_MV(tframe, cp_block_t, y, x);
						for (int k = 0; k < 16; k++)
						{
							for (int l = 0; l < 16; l++)
							{
								temp += abs((int)cp_block_r[k][l] - (int)cp_block_t[k][l]);
							}
						}
						if (temp < diff)
						{
							diff = temp;
							temp_mv_x = mv_x;
							temp_mv_y = mv_y;
						}
					}
				}
			}
			ImageArray[frame_num].moVecX.push_back(temp_mv_x);
			ImageArray[frame_num].moVecY.push_back(temp_mv_y);
		}
	}

	for (int i = 0; i < 16; i++)
		delete[] cp_block_t[i];
	delete[] cp_block_t;
	for (int i = 0; i < 16; i++)
		delete[] cp_block_r[i];
	delete[] cp_block_r;

}

void MCestimate(Img image) {
    short mv_x = 0, mv_y = 0, x = 0, y = 0;

    for (int i = 0; i < 288 / 16; i++)
	{
		for (int j = 0; j < 352 / 16; j++)
		{
			x = j * 16;
			y = i * 16;
			
            mv_x = image.moVecX[22*i+j];
            mv_y = image.moVecX[22*i+j];

            for(int m=0; m<16; m++) {
                for(int n=0; n<16; n++) {
                    sframe[y+m][x+n] = rframe[y+mv_y+m][x+mv_x+n];
                }
            }
		}
	}
}

void DIFFIMAGE(int frame_num) {
    MBArray.clear();
    MB tempMB;

    for(int y=0; y<288; y++) {
        for(int x=0; x<352; x++) {
            dframe[y][x] = tframe[y][x] - sframe[y][x];
        }
    }

    for(int row=0;row<288;row=row+8)
    {
        for(int col=0;col<352;col=col+8)
        {
            for(int i=0;i<8;i++)
            {
                for(int j=0;j<8;j++)
                {
                    tempMB.pixel[i][j] = dframe[row+i][col+j];
                }
            }
            MBArray.push_back(tempMB);
        }
    }

    ImageArray[frame_num].MBArray = MBArray;
    MBArray.clear();
}

void GetImage(int frame_num, bool is_encode) {
    cleanMB();
    MB tempMB;

    get_tframe(ImageArray[frame_num].MBArray);

    for(int y=0; y<288; y++) {
        for(int x=0; x<352; x++) {
            tframe[y][x] += sframe[y][x];
        }
    }

    for(int row=0;row<288;row=row+8)
    {
        for(int col=0;col<352;col=col+8)
        {
            for(int i=0;i<8;i++)
            {
                for(int j=0;j<8;j++)
                {
                    tempMB.pixel[i][j] = tframe[row+i][col+j];
                }
            }
            MBArray.push_back(tempMB);
        }
    }

    if(is_encode) {
        ImageArray[frame_num].MBArray = MBArray;
    }
    else {
        ImageArray2[frame_num].MBArray = MBArray;
    }
    cleanMB();
}

void get_rframe(std::vector<MB> mbarray) {
    int row, col;
    int cnt = 0;
    for(row=0; row<288; row+=8) {
        for(col = 0; col<352; col+=8) {
            for(int y=0; y<8; y++) {
                for(int x=0; x<8; x++) {
                    rframe[row+y][col+x] = mbarray[cnt].pixel[y][x];
                }
            }
            cnt++;
        }
    }
}

void Encoding() {
    
    for(int i=0;i<ImageArray.size();i++) {
        if(i%5 == 0) { // Intra
            DCT(ImageArray[i].MBArray);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            Quantize(ImageArray[i].MBArray, 8);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            VLE(i);
            ImageArray2.push_back(ImageArray[i]);
            IQuantize(ImageArray[i].MBArray, 8);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            IDCT(ImageArray[i].MBArray);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            get_rframe(ImageArray[i].MBArray);
        }
        else { // Inter
            MVpredict(i);
            MCestimate(ImageArray[i]);
            DIFFIMAGE(i);
            DCT(ImageArray[i].MBArray);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            Quantize(ImageArray[i].MBArray,8);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            VLE(i);
            
            ImageArray2.push_back(ImageArray[i]);
            IQuantize(ImageArray[i].MBArray, 8);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            IDCT(ImageArray[i].MBArray);
            ImageArray[i].MBArray = MBArray;
            cleanMB();
            GetImage(i,true);
            get_rframe(ImageArray[i].MBArray);
        }
        std::cout << "Encode frame[" << (i+1) << "]\n";
    }

    std::FILE* pFILE;
    pFILE = fopen("./video.dat","wb");

    for(int i=0; i<code.size();i=i+8) {
        bitfiled bit;
        memset(&bit, 0, sizeof(bitfiled));
        bit.b0 = code[i];
        bit.b1 = code[i+1];
        bit.b2 = code[i+2];
        bit.b3 = code[i+3];
        bit.b4 = code[i+4];
        bit.b5 = code[i+5];
        bit.b6 = code[i+6];
        bit.b7 = code[i+7];
        unsigned char cc = (bit.b0 * 128  + bit.b1 * 64 + bit.b2 * 32 + bit.b3 * 16 + bit.b4 * 8 + bit.b5 * 4 + bit.b6 * 2 + bit.b7);
        fwrite(&cc,1,1,pFILE);
    }
    fclose(pFILE);
}

void Decoding() {
    outpixel temppixel;
    byte* pixels;
    for(int i=0; i<ImageArray2.size(); i++) {
        IVLE(i);
        IQuantize(ImageArray2[i].MBArray, 8);
        ImageArray2[i].MBArray = MBArray;
        cleanMB();
        IDCT(ImageArray2[i].MBArray);
        ImageArray2[i].MBArray = MBArray;
        cleanMB();
        std::cout << "check\n";
        get_tframe(ImageArray2[i].MBArray);

        if(i%5 == 0) { // intra
            writeMB(ImageArray2[i].MBArray, pixels, 352, 288, 3);
            temppixel.outpixels = pixels;
            outpixelArray.push_back(temppixel);
            get_rframe(ImageArray2[i].MBArray);
        }
        else { // inter
            MCestimate(ImageArray2[i]);
            GetImage(i,false);
            writeMB(ImageArray2[i].MBArray, pixels, 352, 288, 3);
            temppixel.outpixels = pixels;
            outpixelArray.push_back(temppixel);
            get_rframe(ImageArray2[i].MBArray);
        }

        std::cout << "Decode frame[" << i << "]\n";
    }
}

int main() {
    // Read
    byte* pixels;
    int32 width;
    int32 height;
    int32 bytesPerPixel;
    FILE* imageFile;
    FILE* outputFile1;
    FILE* outputFile2;
    FILE* outputFile3;
    FILE* outputFile4;
    FILE* outputFile5;
    FILE* outputFile6;
    FILE* outputFile7; 
    FILE* outputFile8;
    FILE* outputFile9;
    FILE* outputFile10;
    FILE* outputFile11;
    FILE* outputFile12;
    FILE* outputFile13;
    FILE* outputFile14;
    FILE* outputFile15;
    FILE* outputFile16;
    FILE* outputFile17;
    FILE* outputFile18;
    FILE* outputFile19;
    FILE* outputFile20;
    FILE* outputFile21;
    FILE* outputFile22;
    FILE* outputFile23;
    FILE* outputFile24;
    FILE* outputFile25;
    Img temp_img;

    ReadImage("./Input/Video_Compression01.bmp", "./Output/Video_Compression01.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile1);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression02.bmp", "./Output/Video_Compression02.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile2);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression03.bmp", "./Output/Video_Compression03.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile3);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression04.bmp", "./Output/Video_Compression04.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile4);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression05.bmp", "./Output/Video_Compression05.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile5);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression06.bmp", "./Output/Video_Compression06.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile6);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression07.bmp", "./Output/Video_Compression07.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile7);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression08.bmp", "./Output/Video_Compression08.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile8);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression09.bmp", "./Output/Video_Compression09.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile9);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression10.bmp", "./Output/Video_Compression10.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile10);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression11.bmp", "./Output/Video_Compression11.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile11);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression12.bmp", "./Output/Video_Compression12.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile12);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression13.bmp", "./Output/Video_Compression13.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile13);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression14.bmp", "./Output/Video_Compression14.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile14);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression15.bmp", "./Output/Video_Compression15.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile15);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression16.bmp", "./Output/Video_Compression16.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile16);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression17.bmp", "./Output/Video_Compression17.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile17);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression18.bmp", "./Output/Video_Compression18.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile18);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression19.bmp", "./Output/Video_Compression19.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile19);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression20.bmp", "./Output/Video_Compression20.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile20);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression21.bmp", "./Output/Video_Compression21.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile21);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression22.bmp", "./Output/Video_Compression22.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile22);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression23.bmp", "./Output/Video_Compression23.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile23);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression24.bmp", "./Output/Video_Compression24.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile24);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    ReadImage("./Input/Video_Compression25.bmp", "./Output/Video_Compression25.bmp", &pixels, &width, &height, &bytesPerPixel, imageFile, outputFile25);
    readMB(pixels,width,height,bytesPerPixel);
    temp_img.MBArray = MBArray;
    cleanMB();
    ImageArray.push_back(temp_img);
    
    std::cout << ImageArray.size() << "\n";
    // H.261
    Encoding();
    Decoding();

    // Decoded Image Write
    WriteImage(outpixelArray[0].outpixels, 352, 288, 3, outputFile1);
    WriteImage(outpixelArray[1].outpixels, 352, 288, 3, outputFile2);
    WriteImage(outpixelArray[2].outpixels, 352, 288, 3, outputFile3);
    WriteImage(outpixelArray[3].outpixels, 352, 288, 3, outputFile4);
    WriteImage(outpixelArray[4].outpixels, 352, 288, 3, outputFile5);
    WriteImage(outpixelArray[5].outpixels, 352, 288, 3, outputFile6);
    WriteImage(outpixelArray[6].outpixels, 352, 288, 3, outputFile7);
    WriteImage(outpixelArray[7].outpixels, 352, 288, 3, outputFile8);
    WriteImage(outpixelArray[8].outpixels, 352, 288, 3, outputFile9);
    WriteImage(outpixelArray[9].outpixels, 352, 288, 3, outputFile10);
    WriteImage(outpixelArray[10].outpixels, 352, 288, 3, outputFile11);
    WriteImage(outpixelArray[11].outpixels, 352, 288, 3, outputFile12);
    WriteImage(outpixelArray[12].outpixels, 352, 288, 3, outputFile13);
    WriteImage(outpixelArray[13].outpixels, 352, 288, 3, outputFile14);
    WriteImage(outpixelArray[14].outpixels, 352, 288, 3, outputFile15);
    WriteImage(outpixelArray[15].outpixels, 352, 288, 3, outputFile16);
    WriteImage(outpixelArray[16].outpixels, 352, 288, 3, outputFile17);
    WriteImage(outpixelArray[17].outpixels, 352, 288, 3, outputFile18);
    WriteImage(outpixelArray[18].outpixels, 352, 288, 3, outputFile19);
    WriteImage(outpixelArray[19].outpixels, 352, 288, 3, outputFile20);
    WriteImage(outpixelArray[20].outpixels, 352, 288, 3, outputFile21);
    WriteImage(outpixelArray[21].outpixels, 352, 288, 3, outputFile22);
    WriteImage(outpixelArray[22].outpixels, 352, 288, 3, outputFile23);
    WriteImage(outpixelArray[23].outpixels, 352, 288, 3, outputFile24);
    WriteImage(outpixelArray[24].outpixels, 352, 288, 3, outputFile25);

    return 0;
}