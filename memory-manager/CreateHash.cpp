#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>

int main()
{
    unsigned char digest[SHA512_DIGEST_LENGTH];
    char string[] = "hello world";

    SHA512((unsigned char*)&string, strlen(string), (unsigned char*)&digest);

    char mdString[SHA512_DIGEST_LENGTH*2+1];

    for(int i = 0; i < SHA512_DIGEST_LENGTH; i++)
         sprintf(&mdString[i*2], "%02x", (unsigned int)digest[i]);

    printf("SHA512 digest: %s\n", mdString);

    return 0;
}
