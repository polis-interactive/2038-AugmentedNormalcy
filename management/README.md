# Running playbook

`
ansible-playbook -i management/playbooks/date.name.ini -u pi -b --private-key=~/.ssh/id_rsa management/playbooks/date.name.yml
`

# Creating ssh keys
`
ssh-keygen -t rsa
ssh-copy-id -i ~/.ssh/id_rsa.pub polis@69.4.20.10
`

# ssh into aws
`
ssh -i aws_ec2_keypair.pem ubuntu@3.87.4.155
`

# start up tinyproxy
`
tinyproxy -d -c management/tinyproxy.conf
`

# connect to proxy from ssh pi
`
export http_proxy=http://69.4.20.10:8888
export https_proxy=http://69.4.20.10:8888
`