#!/usr/bin/python3
import subprocess

cmd = ['lsusb', '-d', '413c:']
call = subprocess.Popen (cmd, stdout=subprocess.PIPE)
out = call.communicate()[0].decode('utf-8')
bus = []
device = []
pid = []
for line in out.split('\n'):
        if not line.strip():
                continue
        bus.append(line.split()[1])
        device.append(line.split()[3].strip(':'))
        pid.append(line.split()[5].split(':')[1])

# record supported devices
for i in range(0, len(bus)):
        basename = "%s.%s" % (bus[i], device[i])
        devname = "/dev/bus/usb/%s/%s" % (bus[i], device[i])

        cmd = ['umockdev-record']
        cmd += [devname]
        print ("Running: %s" % " ".join(cmd))
        call = subprocess.Popen (cmd, stdout=subprocess.PIPE)
        out = call.communicate()[0]
        with open ("%s.umockdev" % basename, 'wb') as wfd:
                wfd.write(out)

        # record ioctls
        cmd = ['umockdev-record']
        cmd += ['--ioctl=%s=%s.ioctl' % (devname, basename)]
        cmd += ['./hidreport', "%s" % pid [i]]
        print ("\nRunning: %s" % " ".join(cmd))
        subprocess.call (cmd)

        # replaying ioctls
        cmd = ['umockdev-run', '--device=%s.umockdev' % basename]
        cmd += ['--ioctl=%s=%s.ioctl' % (devname, basename)]
        cmd += ['./hidreport', "%s" % pid [i]]
        print ("\nRunning: %s" % " ".join(cmd))
        subprocess.call (cmd)
        print ("\n")
