# Running playbook

`
ansible-playbook -i management/playbooks/date.name.ini -u pi -b --private-key=~/.ssh/id_rsa management/playbooks/date.name.yml
`

# Creating ssh keys
`
ssh-keygen -t rsa
ssh-copy-id -i ~/.ssh/id_rsa.pub polis@69.4.20.10
`