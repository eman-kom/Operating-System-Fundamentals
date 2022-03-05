#include "node.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Add in your implementation below to the respective functions
// Feel free to add any headers you deem fit (although you do not need to)

// Inserts a new node with data value at index (counting from head
// starting at 0).
// Note: index is guaranteed to be valid.
void insert_node_at(list *lst, int index, int data) {

	node *newNode = (node *)malloc(sizeof(node));
	newNode -> data = data;

	if (lst -> head == NULL) {
		lst -> head = newNode;
		newNode -> next = newNode;
		return;
	}

	node *currNode = lst -> head;
	if (index == 0) {

		//go to last node
		while (currNode -> next != lst -> head){
			currNode = currNode -> next;
		}

		newNode -> next = lst -> head;
		lst -> head = newNode;

	} else {

		//go to just before the index
		for (int i = 0; i < index-1; i++) {
			currNode = currNode -> next;
		}

		newNode -> next = currNode -> next;
	}

	currNode -> next = newNode;
}

// Deletes node at index (counting from head starting from 0).
// Note: index is guarenteed to be valid.
void delete_node_at(list *lst, int index) {

	//check if nothing in list
	if (lst -> head == NULL) return;

	//check if there is only 1 node in the list
	if (lst -> head == lst -> head -> next) {
		free(lst -> head);
		lst -> head = NULL;
		return;
	}

	node* currNode = lst -> head;
	node* deleteNode = lst -> head;

	//delete first node
	if (index == 0) {
		
		//go to last node
		while (currNode -> next != lst -> head) {
			currNode = currNode -> next;
		}

	} else {

		//stop just before node to be deleted
		deleteNode = currNode -> next;
		for (int i = 0; i < index-1; ++i) {
			currNode = deleteNode;
			deleteNode = deleteNode -> next;
		}
	}

	//check if node to be deleted is the first node
	if (currNode -> next == lst -> head) {
		lst -> head = deleteNode -> next;
	}

	currNode -> next = deleteNode -> next;
	free(deleteNode);
}

// Rotates list by the given offset.
// Note: offset is guarenteed to be non-negative.
void rotate_list(list *lst, int offset) {

	if (lst -> head == NULL) return;

	node *currNode = lst -> head;
	for (int i = 0; i < offset; ++i) {
		currNode = currNode -> next;
	}

	//its a circle, anywhere can be the starting point
	lst -> head = currNode;
}

// Reverses the list, with the original "tail" node
// becoming the new head node.
void reverse_list(list *lst) {

	if (lst -> head == NULL) return;
	
	node *prevNode = lst -> head;
	node *currNode = lst -> head -> next;
	node *nextNode = lst -> head -> next;

	while (currNode != lst -> head) {
		nextNode = nextNode -> next;
		currNode -> next = prevNode;
		prevNode = currNode;
		currNode = nextNode;
	}

	// connect last node to first node
	currNode -> next = prevNode;

	// new start point
	lst -> head = prevNode;
}

// Resets list to an empty state (no nodes) and frees
// any allocated memory in the process
void reset_list(list *lst) {

	if (lst -> head == NULL) return;

	node *firstNode = lst -> head;
	node *deleteNode = firstNode;
	node *currNode = firstNode;

	//stops at last node
	while (currNode -> next != firstNode) {
		currNode = currNode -> next;
		free(deleteNode);
		deleteNode = currNode;
	}

	// free the last node
	free(deleteNode);
	lst -> head = NULL;
}

// Traverses list and applies func on data values of
// all elements in the list.
void map(list *lst, int (*func)(int)) {

	if (lst -> head != NULL) {

		node *currNode = lst -> head;

		//stops at last node
		while (currNode -> next != lst -> head) {
			currNode -> data = func(currNode -> data);
			currNode = currNode -> next;
		}

		currNode -> data = func(currNode -> data);
	}
}

// Traverses list and returns the sum of the data values
// of every node in the list.
long sum_list(list *lst) {

	long ans = 0;

	if (lst -> head != NULL) {

		node *currNode = lst -> head;

		//stops at last node
		while (currNode -> next != lst -> head) {
			ans += currNode -> data;
			currNode = currNode -> next;
		}

		// add the last node
		ans += currNode -> data;
	}

	return ans;
}
