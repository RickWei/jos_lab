#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>


// LAB 6: Your driver code here
int pci_e1000_attach(struct pci_func *pcif){
    
    //exercise3
    pci_func_enable(pcif);
    
    //exercise4
    e1000=mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
    //cprintf("E1000_ADDR=%x\n",e1000);
    
    //exercise5 transmit
    for (int i=0; i<NTXDESC; i++) {
        tx_desc_table[i].addr=0;
        tx_desc_table[i].length=0;
        tx_desc_table[i].cso=0;
        tx_desc_table[i].cmd=(E1000_TXD_CMD_RS>>24);
        tx_desc_table[i].status=E1000_TXD_STAT_DD;
        tx_desc_table[i].css=0;
        tx_desc_table[i].special=0;
    }
    *E1000_REG(E1000_TDBAL)=PADDR(tx_desc_table);
    *E1000_REG(E1000_TDBAH)=0;
    *E1000_REG(E1000_TDLEN)=sizeof(tx_desc_table);
    *E1000_REG(E1000_TDH)=0;
    *E1000_REG(E1000_TDT)=0;
    tdt=E1000_REG(E1000_TDT);
    
    uint32_t tctl=0x0004010A;
    *E1000_REG(E1000_TCTL)=tctl;
    uint32_t tpg=0;
    tpg=10;
    tpg|=4<<10;
    tpg|=6<<20;
    tpg&=0x3FFFFFFF;
    *E1000_REG(E1000_TIPG)=tpg;
    /////////
    
    
    //receive
    *E1000_REG(E1000_RAL)=0x12005452;
    *E1000_REG(E1000_RAH)=0x5634|E1000_RAH_AV;
    *E1000_REG(E1000_RDBAL)=PADDR(rx_desc_table);
    *E1000_REG(E1000_RDBAH)=0;
    *E1000_REG(E1000_RDLEN)=sizeof(rx_desc_table);
    for (int i=0; i<NRXDESC; i++) {
        rx_desc_table[i].addr=page2pa(page_alloc(0))+4;
    }
    *E1000_REG(E1000_RDT)=NRXDESC-1;
    *E1000_REG(E1000_RDH)=0;
    rdt=E1000_REG(E1000_RDT);
    
    uint32_t rflag=0;
    rflag|=E1000_RCTL_EN;
    rflag&=(~E1000_RCTL_DTYP_MASK);
    rflag|=E1000_RCTL_BAM;
    rflag|=E1000_RCTL_SZ_2048;
    rflag|=E1000_RCTL_SECRC;
    *E1000_REG(E1000_RCTL)=rflag;
    
    //panic("not implement\n");
    return 0;
}

int e1000_put_tx_desc(struct tx_desc *td) {
    struct tx_desc *tail=tx_desc_table+(*tdt);
    if (!(tail->status&E1000_TXD_STAT_DD)) {
        return -1;
    }
    *tail=*td;
    *tdt=((*tdt)+1)&(NTXDESC-1);
    return 0;
}

int e1000_get_rx_desc(struct rx_desc *rd) {
    int i=(*rdt+1)&(NRXDESC-1);
    if (!(rx_desc_table[i].status&E1000_RXD_STAT_DD )||!(rx_desc_table[i].status&E1000_RXD_STAT_EOP)) {
        return -1;
    }
    struct rx_desc *head=&rx_desc_table[i];
    uint64_t temp=rd->addr;
    *rd=*head;
    head->addr=temp;
    head->status=0;
    *rdt=i;

    return 0;
}