/*
 * demo_miscdrv.c
 * - Demonstrate how to use a misc driver to easily create a file system link
 *      Author: Brian Fraser
 */
#include <linux/module.h>
#include <linux/miscdevice.h>		// for misc-driver calls.
#include <linux/fs.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
//#error Are we building this?

#define MY_DEVICE_FILE  "morse-code"

/******************************************************
 * LED 
 ******************************************************/
#include <linux/leds.h>

DEFINE_LED_TRIGGER(morse_code);

#define LED_ON_TIME_ms 100
#define LED_OFF_TIME_ms 900

static void Mose_led_turn_on (int time)
{
	led_trigger_event(morse_code, LED_FULL);
	msleep(time);
}

static void Mose_led_turn_off (int time)
{
	led_trigger_event(morse_code, LED_OFF);
	msleep(time);
}

static void led_register(void)
{
    // Setup the trigger's name:
    led_trigger_register_simple("morse-code", &morse_code);
}

static void led_unregister(void)
{
    // Cleanup
    led_trigger_unregister_simple(morse_code);
}


/******************************************************
 * Morse Code 
 ******************************************************/

static int dottime = 500; // ms
module_param(dottime, int, S_IRUGO);
MODULE_PARM_DESC(dottime, "Dot time in ms (basic unit of morse code)");

static unsigned short morsecode_codes[] = {
		0xB800,	// A 1011 1
		0xEA80,	// B 1110 1010 1
		0xEBA0,	// C 1110 1011 101
		0xEA00,	// D 1110 101
		0x8000,	// E 1
		0xAE80,	// F 1010 1110 1
		0xEE80,	// G 1110 1110 1
		0xAA00,	// H 1010 101
		0xA000,	// I 101
		0xBBB8,	// J 1011 1011 1011 1
		0xEB80,	// K 1110 1011 1
		0xBA80,	// L 1011 1010 1
		0xEE00,	// M 1110 111
		0xE800,	// N 1110 1
		0xEEE0,	// O 1110 1110 111
		0xBBA0,	// P 1011 1011 101
		0xEEB8,	// Q 1110 1110 1011 1
		0xBA00,	// R 1011 101
		0xA800,	// S 1010 1
		0xE000,	// T 111
		0xAE00,	// U 1010 111
		0xAB80,	// V 1010 1011 1
		0xBB80,	// W 1011 1011 1
		0xEAE0,	// X 1110 1010 111
		0xEBB8,	// Y 1110 1011 1011 1
		0xEEA0	// Z 1110 1110 101
};

static unsigned int reverseBits(unsigned int num)
{
    unsigned int count = sizeof(num) * 8 - 1;
    unsigned int reverse_num = num;
     
    num >>= 1; 
    while(num)
    {
       reverse_num <<= 1;       
       reverse_num |= num & 1;
       num >>= 1;
       count--;
    }
    reverse_num <<= count;
    reverse_num >>= 16;
    return reverse_num;
}

static void moseCodeGenerator(char c)
{
    
    int index;
    unsigned int mask=0x7;
    unsigned int code;
    if ( 'a' <= c && c <= 'z')
        index = c - 'a';
    else if ('A' <=  c && c <= 'Z')
        index = c - 'A';
    else if (c == ' '){
	    led_trigger_event(morse_code, LED_OFF);
	    msleep(dottime*7);
        return;
    }
    else
        return;

    code = reverseBits(morsecode_codes[index]);

	printk(KERN_INFO "%x\n", code);
    printk(KERN_INFO "%x\n", code & mask);

    
    while (code != 0){
        if ((code & mask) == 0x7){
            Mose_led_turn_on(dottime*3);
            code >>=3;
        }
        else if ((code & mask) == 0x6){
            Mose_led_turn_off(dottime);
            code >>=1;
        }
        else if ((code & mask) == 0x5){
            Mose_led_turn_on(dottime);
            Mose_led_turn_off(dottime);
            code >>=2;
        }
        else if ((code & mask) == 0x2){
            Mose_led_turn_off(dottime);
            code >>=1;
        }
        else if ((code & mask) == 0x1){
            Mose_led_turn_on(dottime);
            code >>=1;
        }
    }
    
    Mose_led_turn_off(dottime*3);    
}

/******************************************************
 * Callbacks
 ******************************************************/

static int my_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "morse_code_miscdrv: In my_open()\n");
	return 0;  // Success
}

static int my_close(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "morse_code_miscdrv: In my_close()\n");
	return 0;  // Success
}

static ssize_t my_read(struct file *file,
		char *buff,	size_t count, loff_t *ppos)
{
	printk(KERN_INFO "morse_code_miscdrv: In my_read()\n");
	return 0;  // # bytes actually read.
}

static ssize_t my_write(struct file *file,
		const char *buff, size_t count, loff_t *ppos)
{
    int i;
    for (i=0; i < count-1; i++){
        char ch;
        if (copy_from_user(&ch, &buff[i], sizeof(ch))){
            return -EFAULT;
        }
        moseCodeGenerator(ch);
    }
	// Return # bytes actually written.
	// Return count here just to make it not call us again!
	return count;
}

static long my_unlocked_ioctl (struct file *file, unsigned int cmd, unsigned long arg)
{
	printk(KERN_INFO "morse_code_miscdrv: In my_unlocked_ioctl()\n");
	return 0;  // Success
}



/******************************************************
 * Misc support
 ******************************************************/
// Callbacks:  (structure defined in /linux/fs.h)
struct file_operations my_fops = {
	.owner    =  THIS_MODULE,
	.open     =  my_open,
	.release  =  my_close,
	.read     =  my_read,
	.write    =  my_write,
	.unlocked_ioctl =  my_unlocked_ioctl
};

// Character Device info for the Kernel:
static struct miscdevice my_miscdevice = {
		.minor    = MISC_DYNAMIC_MINOR,         // Let the system assign one.
		.name     = MY_DEVICE_FILE,             // /dev/.... file.
		.fops     = &my_fops                    // Callback functions.
};


/******************************************************
 * Driver initialization and exit:
 ******************************************************/
static int __init my_init(void)
{
	int ret;
	printk(KERN_INFO "----> more_code_misc driver init(): file /dev/%s.\n", MY_DEVICE_FILE);

	// Register as a misc driver:
	ret = misc_register(&my_miscdevice);

    // LED:
    led_register();

	return ret;
}

static void __exit my_exit(void)
{
	printk(KERN_INFO "<---- morse_code_misc driver exit().\n");

	// Unregister misc driver
	misc_deregister(&my_miscdevice);

    // LED:
    led_unregister();
}

module_init(my_init);
module_exit(my_exit);

MODULE_AUTHOR("Junha Choi");
MODULE_DESCRIPTION("morese code driver");
MODULE_LICENSE("GPL");
