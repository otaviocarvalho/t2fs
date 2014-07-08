#include <stdio.h>
#include <stdlib.h>
#include "../include/t2fs.h"

int main(){
    int handle;
    char *buffer = malloc(sizeof(char)*1024);

    handle = t2fs_create("/dir1/teste_write.pdf");
    printf("handle create %d\n", handle);
    handle = t2fs_open("/dir1/teste_write.pdf");
    printf("handle open %d\n", handle);

    int bytes_written = t2fs_write(handle, "One morning, when Gregor Samsa woke from troubled dreams, he found himself transformed in his bed into a horrible vermin. He lay on his armour-like back, and if he lifted his head a little he could see his brown belly, slightly domed and divided by arches into stiff sections. The bedding was hardly able to cover it and seemed ready to slide off any moment. His many legs, pitifully thin compared with the size of the rest of him, waved about helplessly as he looked. 'What's happened to me?' he thought. It wasn't a dream. His room, a proper human room although a little too small, lay peacefully between its four familiar walls. A collection of textile samples lay spread out on the table - Samsa was a travelling salesman - and above it there hung a picture that he had recently cut out of an illustrated magazine and housed in a nice, gilded frame. It showed a lady fitted out with a fur hat and fur boa who sat upright, raising a heavy fur muff that covered the whole of her lower arm towards the viewer. Gregor then t", 1024);
    printf("bytes escritos: %d\n", bytes_written);

    t2fs_close(handle);
    printf("handle close %d\n", handle);

    handle = t2fs_open("/dir1/teste_write.pdf");
    printf("handle open %d\n", handle);

    int i;
    t2fs_seek(handle, 0);
    t2fs_read(handle, buffer, 1024);
    for (i = 0; i < 1024; i++) {
        printf("%c", buffer[i]);
    }
    printf("\n");

    return 0;
}
