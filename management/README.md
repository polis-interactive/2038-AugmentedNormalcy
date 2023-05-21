# Running playbook

`
ansible-playbook -i management/playbooks/date.name.ini -u pi -b --private-key=~/.ssh/id_rsa management/playbooks/date.name.yml
`