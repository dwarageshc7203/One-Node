#include <QByteArray>
#include <iostream>
int main() {
    QByteArray b;
    quint16 len = 36;
    b.append(static_cast<char>((len >> 8) & 0xFF));
    b.append(static_cast<char>(len & 0xFF));
    std::cout << "Size: " << b.size() << std::endl;
    for(int i=0; i<b.size(); ++i) {
        std::cout << "Byte " << i << ": " << (int)(unsigned char)b[i] << std::endl;
    }
    return 0;
}
