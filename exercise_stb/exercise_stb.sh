# cat *.sh
#!/bin/sh

# run for 5 minutes every half hour

while :
do
  ./exercise_stb -t 5 -r 5

  sleep 1800
done
