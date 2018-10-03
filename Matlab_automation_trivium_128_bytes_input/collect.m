%start timer
tic 

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Parameter setup %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%Serial port of the cryptographic device.  
serialport='COM4';

%keys input file
input_keys='keys.txt';
%ivs input file
input_ivs='ivs.txt';

%output file for encrypted text that is coming from the cryptographic device
outputfile='generated_cipher_text.txt';

%if need to verify cipher text coming from the cryptographic device  by comparing with "verifyfile" set this to 1. Else 0.
verifyen=1;
%if verify set to 1 the file name for test vectors
verifyfile='ciphertest.txt';

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% Cryptosystem interface setup%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%open files
keys=fopen(input_keys);
ivs=fopen(input_ivs);
output=fopen(outputfile,'a');

%set serial port. Change the baud rate if required to match the settings on your cryptographic device
s=serial(serialport);
s.InputBufferSize=128;
%s.OutputBufferSize=128;
s.Terminator='';
set(s,'BaudRate',9600);
fopen(s);
%pause(3);

%read the plain text sample 

fprintf('TRIVIUM encription initialised\n\n');

count=1;
while (feof(keys) == 0) || (feof(ivs)==0) 
    pin_key=strtrim(fgets(keys));
    pin_iv=strtrim(fgets(ivs));
    pin=strcat(pin_key,pin_iv);
    %pause(0.1);
    
    %send the keys+ivs to the device and check whether the cryptosystem prints back correctly
    %useful for checking whether proper communication happens
    fprintf(s,'%s',pin);
    %[pgot,b1]=fscanf(s,'%s');
    
    pause(1);
    
    %inform the cryptographic device to stop the current encryption get ready for the next encryption
    fprintf(s,'%s','z');
    [cout,c1]=fscanf(s,'%s',128);
    %we should receive the cipher text from the cryptographic device at this point
    %if we did not receive properly send 'y to try a reset on the device
    if (c1<128)
        fprintf(s,'%s','y');
    end
    %print the cipher text that received from the cryptographic device
    fprintf(output,'%s\r\n',cout);
    
    fprintf('Set - %d\n',count);
    fprintf('Key - %s\t\t\t IV - %s \n', pin_key, pin_iv);
    fprintf('Cipher text - %s \n\n', cout);
    count = count+1;

end
%time value
timeval=toc;

fprintf('End of ecription\n');
fprintf('Cipher text generated for %d (key,IV) sets\n',count-1);
fprintf('Elapsed time = %0.3f \n',timeval);
%close everything
fclose(s);    
fclose(keys);
fclose(ivs);
fclose(output);